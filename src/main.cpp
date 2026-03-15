#ifndef XPLM301
    #error This project requires the X-Plane 4.2.0 SDK for X-Plane 12
#endif

#include "config.h"
#include "app.h"
#include "appstate.h"
#include "browser.h"
#include "cursor.h"
#include "dataref.h"
#include "path.h"

#include <cstring>

#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>

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
float update(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon);
void menuAction(void* mRef, void* iRef);
void captureVrChanges();
void populateAppsMenu();

XPLMMenuID pluginMenuId = nullptr;
XPLMMenuID appsMenuId = nullptr;

PLUGIN_API int XPluginStart(char * name, char * sig, char * desc)
{
    strcpy(name, FRIENDLY_NAME);
    strcpy(sig, BUNDLE_ID);
    strcpy(desc, "Standalone browser window for X-Plane");
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), FRIENDLY_NAME, nullptr, 1);
    pluginMenuId = XPLMCreateMenu(FRIENDLY_NAME, XPLMFindPluginsMenu(), item, menuAction, nullptr);

    // Apps submenu
    int appsItem = XPLMAppendMenuItem(pluginMenuId, "Apps", nullptr, 1);
    appsMenuId = XPLMCreateMenu("Apps", pluginMenuId, appsItem, menuAction, nullptr);

    XPLMAppendMenuItem(pluginMenuId, "Reload configuration", (void *)"ActionReloadConfig", 0);

    XPLMRegisterFlightLoopCallback(update, REFRESH_INTERVAL_SECONDS_SLOW, nullptr);

    XPluginReceiveMessage(0, XPLM_MSG_PLANE_LOADED, nullptr);

    captureVrChanges();
    initializeCursor();

    debug("Plugin started (version %s)\n", VERSION);

#if DEBUG
    Dataref::getInstance()->createCommand("skyscript/debug/window_to_foreground", "Bring window to front", [](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        auto* state = AppState::getInstance();
        if (state->activeApp && state->activeApp->window) {
            XPLMBringWindowToFront(state->activeApp->window);
        }
    });
#endif

    return 1;
}

PLUGIN_API void XPluginStop(void) {
    XPLMUnregisterFlightLoopCallback(update, nullptr);

    destroyCursor();
    AppState::getInstance()->deinitialize();
    debug("Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    Path::getInstance()->reloadPaths();

    auto* state = AppState::getInstance();
    for (auto* app : state->apps) {
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

            if (AppState::getInstance()->initialize()) {
                populateAppsMenu();
            }
            break;

        case XPLM_MSG_PLANE_CRASHED:
            break;

        case XPLM_MSG_PLANE_UNLOADED:
            if ((intptr_t)params != 0) {
                return;
            }

            // Clear apps submenu
            if (appsMenuId) {
                XPLMClearAllMenuItems(appsMenuId);
            }

            AppState::getInstance()->deinitialize();
            break;

        default:
            break;
    }
}

void menuAction(void* mRef, void* iRef) {
    if (!iRef) {
        return;
    }

    // Check if iRef is one of our known string actions
    auto* state = AppState::getInstance();
    bool isAppPointer = false;
    for (auto* a : state->apps) {
        if (a == iRef) {
            isAppPointer = true;
            break;
        }
    }

    if (!isAppPointer) {
        // It's a string action
        if (strcmp((char *)iRef, "ActionReloadConfig") == 0) {
            if (appsMenuId) {
                XPLMClearAllMenuItems(appsMenuId);
            }
            AppState::getInstance()->reload();
            populateAppsMenu();
            return;
        }

        return;
    }

    // It's an App pointer from the Apps submenu
    App* app = static_cast<App*>(iRef);
    App::current = app;
    if (app->visible) {
        app->hideBrowser();
    }
    else {
        app->showBrowser();
        state->activeApp = app;
        state->checkLatestVersion();
    }
    App::current = nullptr;
}

float update(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon) {
    if (!AppState::getInstance()->pluginInitialized) {
        return REFRESH_INTERVAL_SECONDS_SLOW;
    }

    Dataref::getInstance()->update();
    AppState::getInstance()->update();

    // Check if any app is visible
    bool anyVisible = false;
    for (auto* app : AppState::getInstance()->apps) {
        if (app->visible) {
            anyVisible = true;

            // Sync keyboard focus for visible apps
            App::current = app;
            if (app->browser && app->window) {
                bool hasKeyboardFocus = XPLMHasKeyboardFocus(app->window) != 0;
                if (app->browser->hasInputFocus() != hasKeyboardFocus) {
                    if (app->browser->hasInputFocus()) {
                        app->browser->setFocus(true);
                        XPLMBringWindowToFront(app->window);
                        XPLMTakeKeyboardFocus(app->window);
                    }
                    else {
                        app->browser->setFocus(false);
                        XPLMTakeKeyboardFocus(0);
                    }
                }
            }
            App::current = nullptr;
        }
    }

    if (!anyVisible) {
        return REFRESH_INTERVAL_SECONDS_SLOW;
    }

    return REFRESH_INTERVAL_SECONDS_FAST;
}

void populateAppsMenu() {
    auto* state = AppState::getInstance();
    for (int i = 0; i < static_cast<int>(state->apps.size()); i++) {
        if (i == state->defaultAppsStartIndex && i > 0) {
            XPLMAppendMenuSeparator(appsMenuId);
        }
        XPLMAppendMenuItem(appsMenuId, state->apps[i]->name.c_str(), state->apps[i], 0);
    }
}

void captureVrChanges() {
    Dataref::getInstance()->monitorExistingDataref<bool>("sim/graphics/VR/enabled", [](bool isVrEnabled) {
        auto* state = AppState::getInstance();
        for (auto* app : state->apps) {
            if (app->window) {
                app->applyWindowMode();
            }
        }
        debug("VR is now %s.\n", isVrEnabled ? "enabled" : "disabled");
    });
}
