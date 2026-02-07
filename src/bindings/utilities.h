#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMUtilities.h"
#include <unordered_map>
#include <vector>
#include <mutex>

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane Utilities API (XPLMUtilities.h)
 *
 * Exposes command management and utility functions to JS, including callback support.
 */
class UtilitiesBindings {
public:
    /**
     * @brief Bind Utilities functions to the given JS namespace object
     * @param utilities The JS object to attach functions to
     */
    static void Bind(JSObject& utilities);

    /**
     * @brief Unregister all command handlers and release JS callback references.
     *
     * Must be called before plugin unload while JS runtime is still valid.
     */
    static void Shutdown();

private:
    // Command handler storage: maps refcon to JS callback
    struct CommandHandler {
        XPLMCommandRef command_ref;
        ultralight::JSFunction jsFunction;
        bool before;
    };
    static std::vector<CommandHandler*> handlers_;
    static std::mutex handlers_mutex_;

    // Trampoline for XPLMCommandCallback_f
    static int CommandHandlerTrampoline(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);

    // JS-exposed functions
    static JSValue JS_FindCommand(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_CommandBegin(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_CommandEnd(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_CommandOnce(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_CreateCommand(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_RegisterCommandHandler(const JSObject& thisObject, const JSArgs& args);
};
