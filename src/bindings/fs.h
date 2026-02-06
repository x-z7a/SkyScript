#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "log_msg.h"

#include <string>

using namespace ultralight;

/**
 * @brief JavaScript bindings for filesystem access
 *
 * Provides readFile and writeFile, sandboxed to the app's directory.
 * All paths are resolved relative to the app root; attempts to escape
 * via ".." are rejected.
 */
class FsBindings {
public:
    /**
     * @brief Bind filesystem functions to the given JS namespace object
     * @param fs        The JS object to attach functions to
     * @param app_dir   Absolute path to the app's root directory (sandbox root)
     */
    static void Bind(JSObject& fs, const std::string& app_dir);

private:
    /// Sandbox root – set once per BindToView call
    static std::string app_dir_;

    /**
     * @brief Resolve a user-supplied path against app_dir_ and verify it stays inside the sandbox.
     * @return The resolved absolute path, or an empty string if the path escapes the sandbox.
     */
    static std::string ResolveSandboxedPath(const std::string& relative_path);

    // SkyScript.fs.readFile(path: string): string | null
    static JSValue JS_ReadFile(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.fs.writeFile(path: string, content: string): boolean
    static JSValue JS_WriteFile(const JSObject& thisObject, const JSArgs& args);

    // SkyScript.fs.exists(path: string): boolean
    static JSValue JS_Exists(const JSObject& thisObject, const JSArgs& args);
};
