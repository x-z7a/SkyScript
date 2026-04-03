// Example X-Plane plugin using the SkyScript library.
//
// Demonstrates how to use SkyScript as an imported library to create
// CEF browser windows inside X-Plane. Scans the apps/ directory,
// builds menus, and manages app visibility — all through SkyScript's API.

#ifndef XPLM301
    #error This project requires the X-Plane 4.2.0 SDK for X-Plane 12
#endif

#include "skyscript.h"

#include "config.h"
#include "dataref.h"
#include "path.h"

#include <cstring>
#include <regex>

#include <curl/curl.h>

#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>

#include "json.hpp"
#include "notification.h"

#if IBM
#include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}
#endif

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void* params);
void menuAction(void* mRef, void* iRef);
void populateAppsMenu();
void checkLatestVersion();

XPLMMenuID pluginMenuId = nullptr;
XPLMMenuID appsMenuId = nullptr;
bool pluginInitialized = false;
std::string remoteVersion;

PLUGIN_API int XPluginStart(char * name, char * sig, char * desc)
{
    strcpy(name, FRIENDLY_NAME);
    strcpy(sig, BUNDLE_ID);
    strcpy(desc, "Standalone browser window for X-Plane");
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

    SkyScript::initialize();

    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), FRIENDLY_NAME, nullptr, 1);
    pluginMenuId = XPLMCreateMenu(FRIENDLY_NAME, XPLMFindPluginsMenu(), item, menuAction, nullptr);

    int appsItem = XPLMAppendMenuItem(pluginMenuId, "Apps", nullptr, 1);
    appsMenuId = XPLMCreateMenu("Apps", pluginMenuId, appsItem, menuAction, nullptr);

    XPLMAppendMenuItem(pluginMenuId, "Reload configuration", (void *)"ActionReloadConfig", 0);

    XPluginReceiveMessage(0, XPLM_MSG_PLANE_LOADED, nullptr);

    debug("Plugin started (version %s)\n", VERSION);

    return 1;
}

PLUGIN_API void XPluginStop(void) {
    SkyScript::shutdown();
    pluginInitialized = false;
    debug("Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    Path::getInstance()->reloadPaths();

    for (auto* app : SkyScript::getAppWindows()) {
        if (app->window && app->visible) {
            XPLMBringWindowToFront(app->window);
        }
    }

    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    debug("Disabling plugin...\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void* params) {
    switch (msg) {
        case XPLM_MSG_PLANE_LOADED:
            if ((intptr_t)params != 0) {
                return;
            }

            if (!pluginInitialized && SkyScript::loadAppsFromDirectory()) {
                populateAppsMenu();
                pluginInitialized = true;
            }
            break;

        case XPLM_MSG_PLANE_CRASHED:
            break;

        case XPLM_MSG_PLANE_UNLOADED:
            if ((intptr_t)params != 0) {
                return;
            }

            if (appsMenuId) {
                XPLMClearAllMenuItems(appsMenuId);
            }

            SkyScript::destroyAllAppWindows();
            pluginInitialized = false;
            break;

        default:
            break;
    }
}

void menuAction(void* mRef, void* iRef) {
    if (!iRef) {
        return;
    }

    bool isAppPointer = false;
    for (auto* a : SkyScript::getAppWindows()) {
        if (a == iRef) {
            isAppPointer = true;
            break;
        }
    }

    if (!isAppPointer) {
        if (strcmp((char *)iRef, "ActionReloadConfig") == 0) {
            if (appsMenuId) {
                XPLMClearAllMenuItems(appsMenuId);
            }
            SkyScript::reloadApps();
            populateAppsMenu();
            return;
        }

        return;
    }

    App* app = static_cast<App*>(iRef);
    App::current = app;
    if (app->visible) {
        app->hideBrowser();
    }
    else {
        app->showBrowser();
        SkyScript::setActiveApp(app);
        checkLatestVersion();
    }
    App::current = nullptr;
}

void populateAppsMenu() {
    const auto& apps = SkyScript::getAppWindows();
    bool pastRegular = false;
    for (int i = 0; i < static_cast<int>(apps.size()); i++) {
        if (!pastRegular && apps[i]->isDefault && i > 0) {
            XPLMAppendMenuSeparator(appsMenuId);
            pastRegular = true;
        }
        XPLMAppendMenuItem(appsMenuId, apps[i]->name.c_str(), apps[i], 0);
    }
}

void checkLatestVersion() {
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
            auto* active = SkyScript::getActiveApp();
            if (active) {
                App::current = active;
                active->showNotification(new Notification("Update available", description));
                App::current = nullptr;
            }
        }
    }
    catch (const std::exception& e) {
        debug("Could not fetch latest version information from GitHub. Reason: %s\n", e.what());
        remoteVersion = VERSION;
    }
}
