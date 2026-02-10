#include "utilities.h"
#include "log_msg.h"
#include "xplm_dispatch.h"
#include "../manager.h"
#include <cassert>

std::vector<UtilitiesBindings::CommandHandler*> UtilitiesBindings::handlers_;
std::mutex UtilitiesBindings::handlers_mutex_;

void UtilitiesBindings::Bind(JSObject& utilities) {
    utilities["findCommand"] = JSCallbackWithRetval(JS_FindCommand);
    utilities["commandBegin"] = JSCallbackWithRetval(JS_CommandBegin);
    utilities["commandEnd"] = JSCallbackWithRetval(JS_CommandEnd);
    utilities["commandOnce"] = JSCallbackWithRetval(JS_CommandOnce);
    utilities["createCommand"] = JSCallbackWithRetval(JS_CreateCommand);
    utilities["registerCommandHandler"] = JSCallbackWithRetval(JS_RegisterCommandHandler);
}

JSValue UtilitiesBindings::JS_FindCommand(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) return JSValue();
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    XPLMCommandRef ref = CallOnMainThread([&] { return XPLMFindCommand(name_str.c_str()); });
    return JSValue((double)(uintptr_t)ref);
}

JSValue UtilitiesBindings::JS_CommandBegin(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) return JSValue();
    XPLMCommandRef ref = (XPLMCommandRef)(uintptr_t)args[0].ToNumber();
    CallOnMainThread([&] { XPLMCommandBegin(ref); });
    return JSValue();
}

JSValue UtilitiesBindings::JS_CommandEnd(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) return JSValue();
    XPLMCommandRef ref = (XPLMCommandRef)(uintptr_t)args[0].ToNumber();
    CallOnMainThread([&] { XPLMCommandEnd(ref); });
    return JSValue();
}

JSValue UtilitiesBindings::JS_CommandOnce(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) return JSValue();
    XPLMCommandRef ref = (XPLMCommandRef)(uintptr_t)args[0].ToNumber();
    CallOnMainThread([&] { XPLMCommandOnce(ref); });
    return JSValue();
}

JSValue UtilitiesBindings::JS_CreateCommand(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsString()) return JSValue();
    String name = args[0].ToString();
    String desc = args[1].ToString();
    std::string name_str = name.utf8().data();
    std::string desc_str = desc.utf8().data();
    XPLMCommandRef ref = CallOnMainThread([&] { return XPLMCreateCommand(name_str.c_str(), desc_str.c_str()); });
    return JSValue((double)(uintptr_t)ref);
}

int UtilitiesBindings::CommandHandlerTrampoline(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon) {
    auto* handler = reinterpret_cast<CommandHandler*>(inRefcon);
    if (!handler) return 1;

    JSFunction callback = handler->jsFunction;
    const bool before = handler->before;

    Manager::instance().postToUltralightThread([callback, inCommand, inPhase, before]() mutable
                                               {
        JSArgs jsArgs;
        jsArgs.push_back(JSValue((double)(uintptr_t)inCommand));
        jsArgs.push_back(JSValue((int)inPhase));
        jsArgs.push_back(JSValue(before));

        try
        {
            callback(jsArgs);
        }
        catch (...)
        {
        } },
                                               Manager::UltralightTaskPriority::High);

    return 1;
}

JSValue UtilitiesBindings::JS_RegisterCommandHandler(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsFunction() || !args[2].IsBoolean()) return JSValue();
    XPLMCommandRef ref = (XPLMCommandRef)(uintptr_t)args[0].ToNumber();
    JSFunction fn = args[1].ToFunction();
    bool before = args[2].ToBoolean();
    // Ensure function uses the current main context
    fn.set_context(GetJSContext());
    auto* handler = new CommandHandler{ref, fn, before};
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_.push_back(handler);
    }
    CallOnMainThread([&] { XPLMRegisterCommandHandler(ref, CommandHandlerTrampoline, before ? 1 : 0, handler); });
    return JSValue(true);
}

void UtilitiesBindings::Shutdown() {
    std::vector<CommandHandler*> handlers_to_release;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_to_release.swap(handlers_);
    }

    for (CommandHandler* handler : handlers_to_release) {
        if (!handler) {
            continue;
        }
        if (handler->command_ref) {
            CallOnMainThread([&]
                             { XPLMUnregisterCommandHandler(handler->command_ref,
                                                            CommandHandlerTrampoline,
                                                            handler->before ? 1 : 0,
                                                            handler); });
        }
        delete handler;
    }
}
