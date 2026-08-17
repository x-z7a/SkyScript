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
#include <XPLMUtilities.h>

namespace {
std::vector<App*> managedApps;
App* activeApp = nullptr;

// Whether this image has already been initialized. One plugin initializes
// once, so a second initialize means two plugins share this library. See
// SkyScript::initialize.
bool initialized = false;

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

// The plugin's folder name, used to namespace the commands this consumer owns.
// Per-app commands are already unique because the app id comes from its folder,
// but anything global is not, and X-Plane has one command namespace for the
// whole sim.
std::string pluginNamespace() {
    const std::string& directory = Path::getInstance()->pluginDirectory;
    if (directory.empty()) {
        return "";
    }

    size_t separator = directory.find_last_of("/\\");
    return separator == std::string::npos ? directory : directory.substr(separator + 1);
}

void toggleActiveApp(XPLMCommandPhase inPhase) {
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
}

void createToggleCommand() {
    const std::string prefix = pluginNamespace();

    // Every plugin gets a toggle only it owns. Two SkyScript-based plugins in
    // one sim otherwise both register skyscript/toggle, and since
    // XPLMCreateCommand hands back the existing command rather than failing,
    // they would share one binding that toggles whichever app answered first.
    if (!prefix.empty()) {
        Dataref::getInstance()->createCommand(
            ("skyscript/" + prefix + "/toggle").c_str(),
            "Show or hide the last active app",
            toggleActiveApp);
    }

    // The un-namespaced name stays for anyone who has already bound it to a
    // key, but only the first plugin to claim it gets it. Attaching a second
    // handler is what made one keypress toggle another plugin's window.
    if (XPLMFindCommand("skyscript/toggle") == nullptr) {
        Dataref::getInstance()->createCommand(
            "skyscript/toggle",
            "Show or hide the last active app",
            toggleActiveApp);
    } else if (!prefix.empty()) {
        debug("skyscript/toggle is already owned by another plugin; use skyscript/%s/toggle\n",
              prefix.c_str());
    }
}

float flightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void* inRefcon) {
    Dataref::getInstance()->update();

    bool anyVisible = false;
    for (auto* app : managedApps) {
        app->update();

        // A notification-only window is not "visible" -- its browser is hidden
        // -- but it is on screen and animating, and its notification's timeout
        // and slide-out are driven from update(). Left on the slow interval a
        // toast would outstay its timeout by up to two seconds and then leave
        // an empty window behind it.
        if (app->visible || app->hasNotificationOverlay()) {
            anyVisible = true;
        }

        if (app->visible) {
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
    // A plugin initializes SkyScript exactly once, so a second call without an
    // intervening shutdown means two plugins are running against one copy of
    // this library.
    //
    // That happens because every SkyScript-based plugin ships a library with
    // the same identity -- @rpath/libSkyScriptLib.dylib, SONAME
    // libSkyScriptLib.so, module SkyScriptLib.dll -- and loaders key images by
    // identity rather than by path. The second plugin loaded gets the first
    // one's image, and with it the Path singleton, the app registry and the
    // dataref bindings. It then serves the *other* plugin's apps while
    // reporting success, which is a miserable thing to debug from Log.txt.
    //
    // This cannot be repaired from in here: by the time we notice, both
    // plugins are already bound to one set of globals. So it is called out
    // loudly instead, with the fix, rather than left to look like a working
    // plugin that shows the wrong window.
    if (initialized) {
        debug("ERROR: SkyScript is already initialized in this process.\n");
        debug("ERROR: Two plugins are sharing one copy of the library, so this one\n");
        debug("ERROR: will use the other's plugin directory and apps.\n");
        debug("ERROR: Give each plugin's copy a unique identity -- see\n");
        debug("ERROR: scripts/unique-library-identity.sh in the SkyScript repo.\n");
    }
    initialized = true;

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

void setAssetsPath(const std::string& path) {
    Path::getInstance()->setAssetsPath(path);
}

void setPluginPath(const std::string& path) {
    Path::getInstance()->setPluginPath(path);
}

void shutdown() {
    XPLMUnregisterFlightLoopCallback(flightLoopCallback, nullptr);
    destroyAllAppWindows();
    destroyCursor();
    initialized = false;
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

void showNotification(App* app, const NotificationOptions& options) {
    if (app) {
        app->showNotification(options);
    }
}

void dismissNotification(App* app) {
    if (app) {
        app->dismissNotification();
    }
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

void setLogPrefix(const std::string& prefix) {
    _skyscript_log_prefix() = prefix;
}

}
