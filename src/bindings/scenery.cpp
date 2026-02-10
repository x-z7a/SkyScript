#include "scenery.h"
#include "xplm_dispatch.h"

// Static member definitions
std::unordered_map<std::string, XPLMObjectRef> SceneryBindings::object_cache_;
std::mutex SceneryBindings::cache_mutex_;
std::unordered_map<int, XPLMProbeRef> SceneryBindings::probe_cache_;
int SceneryBindings::next_probe_id_ = 1;

void SceneryBindings::Bind(JSObject& scenery) {
    // Object loading
    scenery["loadObject"] = JSCallbackWithRetval(JS_LoadObject);
    scenery["unloadObject"] = JSCallbackWithRetval(JS_UnloadObject);

    // Terrain probing
    scenery["createProbe"] = JSCallbackWithRetval(JS_CreateProbe);
    scenery["destroyProbe"] = JSCallbackWithRetval(JS_DestroyProbe);
    scenery["probeTerrain"] = JSCallbackWithRetval(JS_ProbeTerrainXYZ);

    // Magnetic variation
    scenery["getMagneticVariation"] = JSCallbackWithRetval(JS_GetMagneticVariation);
    scenery["degTrueToMagnetic"] = JSCallbackWithRetval(JS_DegTrueToDegMagnetic);
    scenery["degMagneticToTrue"] = JSCallbackWithRetval(JS_DegMagneticToDegTrue);
}

// =========================================================================
// Object Loading
// =========================================================================

JSValue SceneryBindings::JS_LoadObject(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: loadObject requires a string path argument");
        return JSValue();
    }
    
    String path = args[0].ToString();
    std::string path_str = path.utf8().data();
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = object_cache_.find(path_str);
        if (it != object_cache_.end()) {
            return JSValue(path.utf8().data());
        }
    }
    
    // Load the object
    XPLMObjectRef obj = CallOnMainThread([&] { return XPLMLoadObject(path_str.c_str()); });
    if (!obj) {
        LogMsg("JSBindings: failed to load object: %s", path_str.c_str());
        return JSValue();
    }
    
    // Cache it
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        object_cache_[path_str] = obj;
    }
    
    LogMsg("JSBindings: loaded object: %s", path_str.c_str());
    return JSValue(path.utf8().data());
}

JSValue SceneryBindings::JS_UnloadObject(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: unloadObject requires a string path argument");
        return JSValue(false);
    }
    
    String path = args[0].ToString();
    std::string path_str = path.utf8().data();
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = object_cache_.find(path_str);
    if (it == object_cache_.end()) {
        LogMsg("JSBindings: object not found for unload: %s", path_str.c_str());
        return JSValue(false);
    }
    
    CallOnMainThread([&] { XPLMUnloadObject(it->second); });
    object_cache_.erase(it);
    
    LogMsg("JSBindings: unloaded object: %s", path_str.c_str());
    return JSValue(true);
}

// =========================================================================
// Terrain Probing
// =========================================================================

JSValue SceneryBindings::JS_CreateProbe(const JSObject& thisObject, const JSArgs& args) {
    int probeType = xplm_ProbeY;
    if (!args.empty() && args[0].IsNumber()) {
        probeType = static_cast<int>(args[0].ToNumber());
    }
    
    XPLMProbeRef probe = CallOnMainThread([&] { return XPLMCreateProbe(static_cast<XPLMProbeType>(probeType)); });
    if (!probe) {
        LogMsg("JSBindings: failed to create terrain probe");
        return JSValue();
    }
    
    int id = next_probe_id_++;
    probe_cache_[id] = probe;
    
    LogMsg("JSBindings: created terrain probe with ID %d", id);
    return JSValue(id);
}

JSValue SceneryBindings::JS_DestroyProbe(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: destroyProbe requires a probe ID argument");
        return JSValue(false);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = probe_cache_.find(id);
    if (it == probe_cache_.end()) {
        LogMsg("JSBindings: probe not found: %d", id);
        return JSValue(false);
    }
    
    CallOnMainThread([&] { XPLMDestroyProbe(it->second); });
    probe_cache_.erase(it);
    
    LogMsg("JSBindings: destroyed probe %d", id);
    return JSValue(true);
}

JSValue SceneryBindings::JS_ProbeTerrainXYZ(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 4 || !args[0].IsNumber() || !args[1].IsNumber() || 
        !args[2].IsNumber() || !args[3].IsNumber()) {
        LogMsg("JSBindings: probeTerrain requires (probeId, x, y, z) arguments");
        return JSValue();
    }
    
    int probeId = static_cast<int>(args[0].ToNumber());
    double x = args[1].ToNumber();
    double y = args[2].ToNumber();
    double z = args[3].ToNumber();
    
    auto it = probe_cache_.find(probeId);
    if (it == probe_cache_.end()) {
        LogMsg("JSBindings: probe not found: %d", probeId);
        return JSValue();
    }
    
    XPLMProbeInfo_t info;
    info.structSize = sizeof(XPLMProbeInfo_t);
    
    XPLMProbeResult result = CallOnMainThread([&]
                                              { return XPLMProbeTerrainXYZ(it->second,
                                                                           static_cast<float>(x),
                                                                           static_cast<float>(y),
                                                                           static_cast<float>(z),
                                                                           &info); });
    
    if (result != xplm_ProbeHitTerrain) {
        JSObject errorResult;
        errorResult["hit"] = JSValue(false);
        errorResult["result"] = JSValue(static_cast<int>(result));
        return JSValue(static_cast<JSObjectRef>(errorResult));
    }
    
    JSObject jsResult;
    jsResult["hit"] = JSValue(true);
    jsResult["x"] = JSValue(info.locationX);
    jsResult["y"] = JSValue(info.locationY);
    jsResult["z"] = JSValue(info.locationZ);
    jsResult["normalX"] = JSValue(info.normalX);
    jsResult["normalY"] = JSValue(info.normalY);
    jsResult["normalZ"] = JSValue(info.normalZ);
    jsResult["velocityX"] = JSValue(info.velocityX);
    jsResult["velocityY"] = JSValue(info.velocityY);
    jsResult["velocityZ"] = JSValue(info.velocityZ);
    jsResult["isWet"] = JSValue(info.is_wet != 0);
    
    return JSValue(static_cast<JSObjectRef>(jsResult));
}

// =========================================================================
// Magnetic Variation
// =========================================================================

JSValue SceneryBindings::JS_GetMagneticVariation(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber()) {
        LogMsg("JSBindings: getMagneticVariation requires (latitude, longitude) arguments");
        return JSValue(0.0);
    }
    
    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();
    
    float variation = CallOnMainThread([&] { return XPLMGetMagneticVariation(latitude, longitude); });
    return JSValue(static_cast<double>(variation));
}

JSValue SceneryBindings::JS_DegTrueToDegMagnetic(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: degTrueToMagnetic requires a heading argument");
        return JSValue(0.0);
    }
    
    float headingTrue = static_cast<float>(args[0].ToNumber());
    float headingMag = CallOnMainThread([&] { return XPLMDegTrueToDegMagnetic(headingTrue); });
    return JSValue(static_cast<double>(headingMag));
}

JSValue SceneryBindings::JS_DegMagneticToDegTrue(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: degMagneticToTrue requires a heading argument");
        return JSValue(0.0);
    }
    
    float headingMag = static_cast<float>(args[0].ToNumber());
    float headingTrue = CallOnMainThread([&] { return XPLMDegMagneticToDegTrue(headingMag); });
    return JSValue(static_cast<double>(headingTrue));
}
