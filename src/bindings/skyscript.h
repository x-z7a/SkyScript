#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "log_msg.h"

#include <string>

using namespace ultralight;

/**
 * @brief JavaScript bindings for SkyScript App Management API
 *
 * Provides app listing, reloading, and window management.
 */
class SkyScriptBindings {
public:
    /**
     * @brief Bind SkyScript app management functions to the given JS namespace object
     * @param skyscript The JS object to attach functions to
     */
    static void Bind(JSObject& skyscript);

private:
    static JSValue JS_ListApps(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_ReloadApp(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_OpenAppWindow(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_OpenAppInspector(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief SkyScript.setAppRenderer(name, renderer)
     *
     * Writes the "renderer" key to the named app's manifest.json and
     * recreates the app window with the new renderer.
     *
     * @param name      App name string (e.g. "hello-world")
     * @param renderer  Renderer string: "ultralight" or "cef"
     * @returns true on success, false if the app was not found or the
     *          renderer string is invalid.
     *
     * Note: calling this with "cef" when SKYSCRIPT_CEF_ENABLED is not defined
     * is safe — the manifest is updated but the app falls back to Ultralight.
     */
    static JSValue JS_SetAppRenderer(const JSObject& thisObject, const JSArgs& args);
};
