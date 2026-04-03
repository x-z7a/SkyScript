#include "skyscript.h"

#include "browser.h"
#include "include/config.h"
#include "include/utils/cursor/cursor.h"
#include "include/utils/dataref.h"
#include "include/utils/path.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <XPLMDisplay.h>
#include <XPLMProcessing.h>

namespace {
std::vector<App*> managedApps;
App* activeApp = nullptr;

void scanApps() {
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

            std::ifstream file(manifestPath);
            std::string line;
            while (std::getline(file, line)) {
                size_t colonPos = line.find(':');
                if (colonPos == std::string::npos) {
                    continue;
                }

                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                auto ks = key.find_first_not_of(" \t");
                auto ke = key.find_last_not_of(" \t");
                if (ks != std::string::npos) {
                    key = key.substr(ks, ke - ks + 1);
                }

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
        App* app = SkyScript::createAppWindow(appName, appId, appConfig);
        app->isDefault = appIsDefault;

        if (appIsDefault) {
            defaultApps.push_back(app);
        } else {
            regularApps.push_back(app);
        }

        debug("Discovered app: %s (id: %s, default: %s)\n", appName.c_str(), appId.c_str(), appIsDefault ? "yes" : "no");
    }

    // Regular apps first, then default apps
    std::vector<App*> ordered;
    for (auto* app : regularApps) {
        ordered.push_back(app);
    }
    for (auto* app : defaultApps) {
        ordered.push_back(app);
    }

    // Reorder managedApps to match the scan order
    // (createAppWindow already appended them, so rebuild)
    managedApps.clear();
    managedApps = ordered;
}

void createToggleCommand() {
    Dataref::getInstance()->createCommand("skyscript/toggle", "Show or hide the last active app", [](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (activeApp) {
            if (activeApp->visible) {
                activeApp->hideBrowser();
            } else {
                activeApp->showBrowser();
            }
        } else if (!managedApps.empty()) {
            managedApps[0]->showBrowser();
            activeApp = managedApps[0];
        }
    });
}

float flightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void* inRefcon) {
    Dataref::getInstance()->update();

    bool anyVisible = false;
    for (auto* app : managedApps) {
        app->update();

        if (app->visible) {
            anyVisible = true;

            App::current = app;
            if (app->browser && app->window) {
                bool hasKeyboardFocus = XPLMHasKeyboardFocus(app->window) != 0;
                if (app->browser->hasInputFocus() != hasKeyboardFocus) {
                    if (app->browser->hasInputFocus()) {
                        app->browser->setFocus(true);
                        XPLMBringWindowToFront(app->window);
                        XPLMTakeKeyboardFocus(app->window);
                    } else {
                        app->browser->setFocus(false);
                        XPLMTakeKeyboardFocus(0);
                    }
                }
            }
            App::current = nullptr;
        }
    }

    return anyVisible ? REFRESH_INTERVAL_SECONDS_FAST : REFRESH_INTERVAL_SECONDS_SLOW;
}

} // anonymous namespace

namespace SkyScript {

void initialize() {
    Path::getInstance()->reloadPaths();
    initializeCursor();
    XPLMRegisterFlightLoopCallback(flightLoopCallback, REFRESH_INTERVAL_SECONDS_SLOW, nullptr);

    Dataref::getInstance()->monitorExistingDataref<bool>("sim/graphics/VR/enabled", [](bool isVrEnabled) {
        for (auto* app : managedApps) {
            if (app->window) {
                app->applyWindowMode();
            }
        }
        debug("VR is now %s.\n", isVrEnabled ? "enabled" : "disabled");
    });
}

void shutdown() {
    XPLMUnregisterFlightLoopCallback(flightLoopCallback, nullptr);
    destroyAllAppWindows();
    destroyCursor();
}

bool loadAppsFromDirectory() {
    Path::getInstance()->reloadPaths();
    if (Path::getInstance()->pluginDirectory.empty()) {
        return false;
    }

    scanApps();
    createToggleCommand();

    if (!managedApps.empty() && !activeApp) {
        activeApp = managedApps[0];
    }

    return !managedApps.empty();
}

void reloadApps() {
    debug("Reloading apps...\n");

    destroyAllAppWindows();
    scanApps();
    createToggleCommand();

    if (!managedApps.empty() && !activeApp) {
        activeApp = managedApps[0];
    }

    debug("Reloaded. %zu app(s) discovered.\n", managedApps.size());
}

App* createAppWindow(const std::string& name, const std::string& id, const AppConfiguration& config) {
    App* app = new App(name, id, AppType::Folder, config);
    app->initialize();
    app->registerWindow();
    managedApps.push_back(app);
    return app;
}

void destroyAppWindow(App* app) {
    if (app == activeApp) {
        activeApp = nullptr;
    }
    auto it = std::find(managedApps.begin(), managedApps.end(), app);
    if (it != managedApps.end()) {
        managedApps.erase(it);
    }
    app->deinitialize();
    delete app;
}

const std::vector<App*>& getAppWindows() {
    return managedApps;
}

App* getActiveApp() {
    return activeApp;
}

void setActiveApp(App* app) {
    activeApp = app;
}

App* findApp(const std::string& id) {
    for (auto* app : managedApps) {
        if (app->id == id) {
            return app;
        }
    }
    return nullptr;
}

void destroyAllAppWindows() {
    Dataref::getInstance()->destroyAllBindings();
    for (auto* app : managedApps) {
        app->deinitialize();
        delete app;
    }
    managedApps.clear();
    activeApp = nullptr;
}

}
