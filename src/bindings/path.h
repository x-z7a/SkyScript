#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "log_msg.h"

#include <string>

using namespace ultralight;

/**
 * @brief JavaScript bindings for cross-platform path utilities
 *
 * Provides join, dirname, basename, extname, and the platform separator.
 * All operations are pure string manipulation – no filesystem access.
 */
class PathBindings {
public:
    /**
     * @brief Bind path utility functions to the given JS namespace object
     * @param path  The JS object to attach functions to
     */
    static void Bind(JSObject& path);

private:
    // SkyScript.path.join(...parts: string[]): string
    static JSValue JS_Join(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.path.dirname(path: string): string
    static JSValue JS_Dirname(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.path.basename(path: string, ext?: string): string
    static JSValue JS_Basename(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.path.extname(path: string): string
    static JSValue JS_Extname(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.path.normalize(path: string): string
    static JSValue JS_Normalize(const JSObject& thisObject, const JSArgs& args);
};
