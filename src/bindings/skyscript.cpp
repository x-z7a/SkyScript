#include "skyscript.h"
#include "../manager.h"
#include "xplm_dispatch.h"

void SkyScriptBindings::Bind(JSObject& skyscript) {
    skyscript["listApps"] = JSCallbackWithRetval(JS_ListApps);
    skyscript["reloadApp"] = JSCallbackWithRetval(JS_ReloadApp);
    skyscript["openAppWindow"] = JSCallbackWithRetval(JS_OpenAppWindow);
    skyscript["openAppInspector"] = JSCallbackWithRetval(JS_OpenAppInspector);
}

JSValue SkyScriptBindings::JS_ListApps(const JSObject& thisObject, const JSArgs& args) {
    auto names = CallOnMainThread([&] { return Manager::instance().getAppNames(); });
    
    JSArray result;
    for (size_t i = 0; i < names.size(); i++) {
        result.push(JSValue(names[i].c_str()));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue SkyScriptBindings::JS_ReloadApp(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: reloadApp requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(CallOnMainThread([&] { return Manager::instance().reloadApp(name_str); }));
}

JSValue SkyScriptBindings::JS_OpenAppWindow(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: openAppWindow requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(CallOnMainThread([&] { return Manager::instance().openAppWindow(name_str); }));
}

JSValue SkyScriptBindings::JS_OpenAppInspector(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: openAppInspector requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(CallOnMainThread([&] { return Manager::instance().openAppInspector(name_str); }));
}
