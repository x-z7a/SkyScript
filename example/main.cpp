// Example X-Plane plugin using the SkyScript shared library via C API.
//
// Demonstrates how to use SkyScript as a shared library (.dll/.dylib/.so)
// to create CEF browser windows inside X-Plane. Scans the apps/ directory,
// builds menus, and manages app visibility — all through SkyScript's C API.
//
// Using the C API (skyscript_c.h) avoids C++ ABI coupling, allowing this
// plugin to be built with any toolchain (MSVC, MinGW, GCC, Clang).

#ifndef XPLM301
    #error This project requires the X-Plane 4.2.0 SDK for X-Plane 12
#endif

#include "skyscript_c.h"

#include "config.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <regex>

#include <curl/curl.h>

#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>

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

static void pluginLog(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int offset = snprintf(buffer, sizeof(buffer), "[%s] ", FRIENDLY_NAME);
    vsnprintf(buffer + offset, sizeof(buffer) - static_cast<size_t>(offset), format, args);
    va_end(args);
    XPLMDebugString(buffer);
}

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

    skyscript_initialize();

    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), FRIENDLY_NAME, nullptr, 1);
    pluginMenuId = XPLMCreateMenu(FRIENDLY_NAME, XPLMFindPluginsMenu(), item, menuAction, nullptr);

    int appsItem = XPLMAppendMenuItem(pluginMenuId, "Apps", nullptr, 1);
    appsMenuId = XPLMCreateMenu("Apps", pluginMenuId, appsItem, menuAction, nullptr);

    XPLMAppendMenuItem(pluginMenuId, "Reload configuration", (void *)"ActionReloadConfig", 0);

    XPluginReceiveMessage(0, XPLM_MSG_PLANE_LOADED, nullptr);

    pluginLog("Plugin started (version %s)\n", VERSION);

    return 1;
}

PLUGIN_API void XPluginStop(void) {
    skyscript_shutdown();
    pluginInitialized = false;
    pluginLog("Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    pluginLog("Disabling plugin...\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void* params) {
    switch (msg) {
        case XPLM_MSG_PLANE_LOADED:
            if ((intptr_t)params != 0) {
                return;
            }

            if (!pluginInitialized && skyscript_load_apps_from_directory()) {
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

            skyscript_destroy_all_app_windows();
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

    if (strcmp((char *)iRef, "ActionReloadConfig") == 0) {
        if (appsMenuId) {
            XPLMClearAllMenuItems(appsMenuId);
        }
        skyscript_reload_apps();
        populateAppsMenu();
        return;
    }

    SkyScriptApp app = (SkyScriptApp)iRef;
    if (skyscript_app_is_visible(app)) {
        skyscript_app_hide(app);
    } else {
        skyscript_app_show(app, NULL);
        skyscript_set_active_app(app);
        checkLatestVersion();
    }
}

void populateAppsMenu() {
    int count = skyscript_get_app_window_count();
    for (int i = 0; i < count; i++) {
        SkyScriptApp app = skyscript_get_app_window_at(i);
        if (app) {
            XPLMAppendMenuItem(appsMenuId, skyscript_app_get_name(app), app, 0);
        }
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
        pluginLog("Version fetch failed: %s\n", curl_easy_strerror(status));
    }
    curl_easy_cleanup(curl);

    remoteVersion = response.empty() ? VERSION : VERSION;
    pluginLog("Version check complete.\n");
}
