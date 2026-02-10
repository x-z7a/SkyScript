#include "graphics.h"
#include "xplm_dispatch.h"

void GraphicsBindings::Bind(JSObject& graphics) {
    graphics["localToWorld"] = JSCallbackWithRetval(JS_LocalToWorld);
    graphics["worldToLocal"] = JSCallbackWithRetval(JS_WorldToLocal);
}

JSValue GraphicsBindings::JS_LocalToWorld(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        LogMsg("JSBindings: localToWorld requires (x, y, z) arguments");
        return JSValue();
    }
    
    double x = args[0].ToNumber();
    double y = args[1].ToNumber();
    double z = args[2].ToNumber();
    
    double latitude, longitude, altitude;
    CallOnMainThread([&] { XPLMLocalToWorld(x, y, z, &latitude, &longitude, &altitude); });
    
    JSObject result;
    result["latitude"] = JSValue(latitude);
    result["longitude"] = JSValue(longitude);
    result["altitude"] = JSValue(altitude);
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue GraphicsBindings::JS_WorldToLocal(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        LogMsg("JSBindings: worldToLocal requires (latitude, longitude, altitude) arguments");
        return JSValue();
    }
    
    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();
    double altitude = args[2].ToNumber();
    
    double x, y, z;
    CallOnMainThread([&] { XPLMWorldToLocal(latitude, longitude, altitude, &x, &y, &z); });
    
    JSObject result;
    result["x"] = JSValue(x);
    result["y"] = JSValue(y);
    result["z"] = JSValue(z);
    
    return JSValue(static_cast<JSObjectRef>(result));
}
