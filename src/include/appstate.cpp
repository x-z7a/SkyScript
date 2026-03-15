#include "appstate.h"

#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>

#include <curl/curl.h>

#include "browser.h"
#include "config.h"
#include "dataref.h"
#include "json.hpp"
#include "notification.h"
#include "path.h"

AppState* AppState::instance = nullptr;

AppState::AppState() {
    remoteVersion = "";
    pluginInitialized = false;
    activeApp = nullptr;
}

AppState::~AppState() {
    instance = nullptr;
}

AppState* AppState::getInstance() {
    if (instance == nullptr) {
        instance = new AppState();
    }

    return instance;
}

bool AppState::initialize() {
    if (pluginInitialized) {
        return false;
    }

    Path::getInstance()->reloadPaths();
    if (Path::getInstance()->pluginDirectory.empty()) {
        return false;
    }

    scanApps();

    // Create global toggle command for backward compatibility
    Dataref::getInstance()->createCommand("skyscript/toggle", "Show or hide the last active app", [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (activeApp) {
            if (activeApp->visible) {
                activeApp->hideBrowser();
            }
            else {
                activeApp->showBrowser();
            }
        }
        else if (!apps.empty()) {
            apps[0]->showBrowser();
            activeApp = apps[0];
        }
    });

    // Initialize all apps
    for (auto* app : apps) {
        app->initialize();
        app->registerWindow();
    }

    if (!apps.empty() && !activeApp) {
        activeApp = apps[0];
    }

    pluginInitialized = true;
    return true;
}

void AppState::deinitialize() {
    if (!pluginInitialized) {
        return;
    }

    Dataref::getInstance()->destroyAllBindings();

    for (auto* app : apps) {
        app->deinitialize();
        delete app;
    }
    apps.clear();
    activeApp = nullptr;

    pluginInitialized = false;
}

void AppState::checkLatestVersion() {
    if (!remoteVersion.empty()) {
        return;
    }

    std::string response;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, VERSION_CHECK_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    CURLcode status = curl_easy_perform(curl);
    if (status != CURLE_OK) {
        debug("Version fetch failed: %s\n", curl_easy_strerror(status));
    }
    curl_easy_cleanup(curl);

    try {
        std::string tag = nlohmann::json::parse(response)[0]["tag_name"];
        if (tag.starts_with("v")) {
            tag = tag.substr(1);
        }

        remoteVersion = tag;
        std::string cleanedRemote = std::regex_replace(tag, std::regex("[^0-9]"), "");
        std::string cleanedLocal = std::regex_replace(VERSION, std::regex("[^0-9]"), "");
        int remoteVersionNumber = std::stoi(cleanedRemote);
        int localVersionNumber = std::stoi(cleanedLocal);
        if (remoteVersionNumber > localVersionNumber) {
            debug("There is a newer version of the plugin available. Current: %s, latest: %s\n", VERSION, tag.c_str());
            std::string description = "There is an update available for the " + std::string(FRIENDLY_NAME) + " plugin.\n\nVersion " + tag + ".\n";
            if (activeApp) {
                App::current = activeApp;
                activeApp->showNotification(new Notification("Update available", description));
                App::current = nullptr;
            }
        }
    }
    catch (const std::exception& e) {
        debug("Could not fetch latest version information from GitHub. Reason: %s\n", e.what());
        remoteVersion = VERSION;
    }
}

void AppState::update() {
    for (auto* app : apps) {
        app->update();
    }
}

void AppState::scanApps() {
    std::string pluginDir = Path::getInstance()->pluginDirectory;
    std::string appsDir = pluginDir + "/" + APPS_DIRECTORY;

    if (!std::filesystem::exists(appsDir) || !std::filesystem::is_directory(appsDir)) {
        return;
    }

    std::vector<App*> regularApps;
    std::vector<App*> defaultApps;

    for (const auto& entry : std::filesystem::directory_iterator(appsDir)) {
        if (!entry.is_directory()) {
            continue;
        }

        std::string folderName = entry.path().filename().string();
        std::string manifestPath = entry.path().string() + "/manifest.yaml";
        std::string indexPath = entry.path().string() + "/index.html";

        AppConfiguration appConfig = App::defaultConfig();
        std::string appName = folderName;
        bool appIsDefault = false;

        if (std::filesystem::exists(manifestPath)) {
            appConfig = App::parseManifest(manifestPath, App::defaultConfig());

            // Parse name and default flag from manifest
            std::ifstream file(manifestPath);
            std::string line;
            while (std::getline(file, line)) {
                size_t colonPos = line.find(':');
                if (colonPos == std::string::npos) {
                    continue;
                }

                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // Trim key
                auto ks = key.find_first_not_of(" \t");
                auto ke = key.find_last_not_of(" \t");
                if (ks != std::string::npos) {
                    key = key.substr(ks, ke - ks + 1);
                }

                // Trim value
                auto vs = value.find_first_not_of(" \t");
                auto ve = value.find_last_not_of(" \t\r\n");
                if (vs != std::string::npos) {
                    value = value.substr(vs, ve - vs + 1);
                } else {
                    value = "";
                }

                if (key == "name" && !value.empty()) {
                    appName = value;
                } else if (key == "default") {
                    appIsDefault = (value == "true" || value == "1");
                }
            }
        }

        if (appConfig.homepage.empty() || appConfig.homepage == App::defaultConfig().homepage) {
            if (std::filesystem::exists(indexPath)) {
                appConfig.homepage = "file://" + indexPath;
            }
        }

        std::string appId = "app-" + folderName;
        App* app = new App(appName, appId, AppType::Folder, appConfig);
        app->isDefault = appIsDefault;

        if (appIsDefault) {
            defaultApps.push_back(app);
        } else {
            regularApps.push_back(app);
        }

        debug("Discovered app: %s (id: %s, default: %s)\n", appName.c_str(), appId.c_str(), appIsDefault ? "yes" : "no");
    }

    // Regular apps first, then default apps (with separator between them in menu)
    for (auto* app : regularApps) {
        apps.push_back(app);
    }
    defaultAppsStartIndex = static_cast<int>(apps.size());
    for (auto* app : defaultApps) {
        apps.push_back(app);
    }
}

App* AppState::findApp(const std::string& id) {
    for (auto* app : apps) {
        if (app->id == id) {
            return app;
        }
    }
    return nullptr;
}

void AppState::reload() {
    if (!pluginInitialized) {
        return;
    }

    debug("Reloading plugin...\n");

    // Tear down all existing apps
    Dataref::getInstance()->destroyAllBindings();

    for (auto* app : apps) {
        app->deinitialize();
        delete app;
    }
    apps.clear();
    activeApp = nullptr;

    // Rescan apps
    scanApps();

    // Re-create global toggle command
    Dataref::getInstance()->createCommand("skyscript/toggle", "Show or hide the last active app", [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (activeApp) {
            if (activeApp->visible) {
                activeApp->hideBrowser();
            }
            else {
                activeApp->showBrowser();
            }
        }
        else if (!apps.empty()) {
            apps[0]->showBrowser();
            activeApp = apps[0];
        }
    });

    // Initialize and register all apps
    for (auto* app : apps) {
        app->initialize();
        app->registerWindow();
    }

    if (!apps.empty() && !activeApp) {
        activeApp = apps[0];
    }

    debug("Plugin reloaded. %zu app(s) discovered.\n", apps.size());
}

