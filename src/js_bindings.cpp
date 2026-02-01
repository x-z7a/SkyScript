#include "js_bindings.h"

// Static member definitions
std::unordered_map<std::string, XPLMDataRef> JSBindings::dataref_cache_;
std::mutex JSBindings::cache_mutex_;

// Scenery/Instance static members
std::unordered_map<std::string, XPLMObjectRef> JSBindings::object_cache_;
std::unordered_map<int, XPLMInstanceRef> JSBindings::instance_cache_;
int JSBindings::next_instance_id_ = 1;
std::unordered_map<int, XPLMProbeRef> JSBindings::probe_cache_;
int JSBindings::next_probe_id_ = 1;

XPLMDataRef JSBindings::GetCachedDataRef(const std::string& name) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = dataref_cache_.find(name);
    if (it != dataref_cache_.end()) {
        return it->second;
    }
    
    XPLMDataRef ref = XPLMFindDataRef(name.c_str());
    if (ref) {
        dataref_cache_[name] = ref;
    }
    return ref;
}

void JSBindings::BindToView(RefPtr<View> view) {
    RefPtr<JSContext> context = view->LockJSContext();
    SetJSContext(context->ctx());
    JSObject global = JSGlobalObject();
    
    // Create the XPlane namespace object
    JSObject xplane;
    
    // Create the dataref sub-namespace
    JSObject dataref;
    
    // Bind all DataRef functions using JSCallbackWithRetval directly (for static functions)
    dataref["find"] = JSCallbackWithRetval(JS_FindDataRef);
    dataref["canWrite"] = JSCallbackWithRetval(JS_CanWriteDataRef);
    dataref["getTypes"] = JSCallbackWithRetval(JS_GetDataRefTypes);
    
    // Getters
    dataref["getInt"] = JSCallbackWithRetval(JS_GetDatai);
    dataref["getFloat"] = JSCallbackWithRetval(JS_GetDataf);
    dataref["getDouble"] = JSCallbackWithRetval(JS_GetDatad);
    dataref["getIntArray"] = JSCallbackWithRetval(JS_GetDatavi);
    dataref["getFloatArray"] = JSCallbackWithRetval(JS_GetDatavf);
    dataref["getData"] = JSCallbackWithRetval(JS_GetDatab);
    
    // Setters
    dataref["setInt"] = JSCallbackWithRetval(JS_SetDatai);
    dataref["setFloat"] = JSCallbackWithRetval(JS_SetDataf);
    dataref["setDouble"] = JSCallbackWithRetval(JS_SetDatad);
    dataref["setIntArray"] = JSCallbackWithRetval(JS_SetDatavi);
    dataref["setFloatArray"] = JSCallbackWithRetval(JS_SetDatavf);
    dataref["setData"] = JSCallbackWithRetval(JS_SetDatab);
    
    // Attach dataref namespace to XPlane
    xplane["dataref"] = JSValue(static_cast<JSObjectRef>(dataref));
    
    // =========================================================================
    // Create the scenery sub-namespace
    // =========================================================================
    JSObject scenery;
    
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
    
    xplane["scenery"] = JSValue(static_cast<JSObjectRef>(scenery));
    
    // =========================================================================
    // Create the instance sub-namespace
    // =========================================================================
    JSObject instance;
    
    instance["create"] = JSCallbackWithRetval(JS_CreateInstance);
    instance["destroy"] = JSCallbackWithRetval(JS_DestroyInstance);
    instance["setPosition"] = JSCallbackWithRetval(JS_InstanceSetPosition);
    
    xplane["instance"] = JSValue(static_cast<JSObjectRef>(instance));
    
    // =========================================================================
    // Create the graphics sub-namespace
    // =========================================================================
    JSObject graphics;
    
    graphics["localToWorld"] = JSCallbackWithRetval(JS_LocalToWorld);
    graphics["worldToLocal"] = JSCallbackWithRetval(JS_WorldToLocal);
    
    xplane["graphics"] = JSValue(static_cast<JSObjectRef>(graphics));
    
    // Attach XPlane to global
    global["XPlane"] = JSValue(static_cast<JSObjectRef>(xplane));
    
    LogMsg("JSBindings: Bound XPlane API (dataref, scenery, instance, graphics) to view");
}

// =========================================================================
// DataRef Lookup Functions
// =========================================================================

JSValue JSBindings::JS_FindDataRef(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: findDataRef requires a string argument");
        return JSValue();
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (ref) {
        return JSValue(true);
    }
    return JSValue();
}

JSValue JSBindings::JS_CanWriteDataRef(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: canWriteDataRef requires a string argument");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        return JSValue(false);
    }
    
    return JSValue(XPLMCanWriteDataRef(ref) != 0);
}

JSValue JSBindings::JS_GetDataRefTypes(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getDataRefTypes requires a string argument");
        return JSValue();
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        return JSValue();
    }
    
    XPLMDataTypeID types = XPLMGetDataRefTypes(ref);
    
    JSObject result;
    result["int"] = JSValue((types & xplmType_Int) != 0);
    result["float"] = JSValue((types & xplmType_Float) != 0);
    result["double"] = JSValue((types & xplmType_Double) != 0);
    result["intArray"] = JSValue((types & xplmType_IntArray) != 0);
    result["floatArray"] = JSValue((types & xplmType_FloatArray) != 0);
    result["data"] = JSValue((types & xplmType_Data) != 0);
    
    return JSValue(static_cast<JSObjectRef>(result));
}

// =========================================================================
// Data Getters
// =========================================================================

JSValue JSBindings::JS_GetDatai(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getInt requires a string argument");
        return JSValue(0);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0);
    }
    
    return JSValue(XPLMGetDatai(ref));
}

JSValue JSBindings::JS_GetDataf(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getFloat requires a string argument");
        return JSValue(0.0);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0.0);
    }
    
    return JSValue(static_cast<double>(XPLMGetDataf(ref)));
}

JSValue JSBindings::JS_GetDatad(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getDouble requires a string argument");
        return JSValue(0.0);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0.0);
    }
    
    return JSValue(XPLMGetDatad(ref));
}

JSValue JSBindings::JS_GetDatavi(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getIntArray requires a string argument");
        return JSValue();
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue();
    }
    
    // Get array size
    int size = XPLMGetDatavi(ref, nullptr, 0, 0);
    if (size <= 0) {
        return JSValue();
    }
    
    // Parse optional offset and count
    int offset = 0;
    int count = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        count = static_cast<int>(args[2].ToNumber());
    }
    
    // Clamp values
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue();
    if (count > size - offset) count = size - offset;
    
    // Read the data
    std::vector<int> values(count);
    XPLMGetDatavi(ref, values.data(), offset, count);
    
    // Convert to JS array
    JSArray result;
    for (int i = 0; i < count; i++) {
        result.push(JSValue(values[i]));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_GetDatavf(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getFloatArray requires a string argument");
        return JSValue();
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue();
    }
    
    // Get array size
    int size = XPLMGetDatavf(ref, nullptr, 0, 0);
    if (size <= 0) {
        return JSValue();
    }
    
    // Parse optional offset and count
    int offset = 0;
    int count = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        count = static_cast<int>(args[2].ToNumber());
    }
    
    // Clamp values
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue();
    if (count > size - offset) count = size - offset;
    
    // Read the data
    std::vector<float> values(count);
    XPLMGetDatavf(ref, values.data(), offset, count);
    
    // Convert to JS array
    JSArray result;
    for (int i = 0; i < count; i++) {
        result.push(JSValue(static_cast<double>(values[i])));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_GetDatab(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: getData requires a string argument");
        return JSValue("");
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue("");
    }
    
    // Get data size
    int size = XPLMGetDatab(ref, nullptr, 0, 0);
    if (size <= 0) {
        return JSValue("");
    }
    
    // Parse optional offset and maxBytes
    int offset = 0;
    int maxBytes = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        maxBytes = static_cast<int>(args[2].ToNumber());
    }
    
    // Clamp values
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue("");
    if (maxBytes > size - offset) maxBytes = size - offset;
    
    // Read the data
    std::vector<char> buffer(maxBytes + 1, 0);
    XPLMGetDatab(ref, buffer.data(), offset, maxBytes);
    
    return JSValue(buffer.data());
}

// =========================================================================
// Data Setters
// =========================================================================

JSValue JSBindings::JS_SetDatai(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber()) {
        LogMsg("JSBindings: setInt requires (string, number) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    int value = static_cast<int>(args[1].ToNumber());
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    XPLMSetDatai(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDataf(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber()) {
        LogMsg("JSBindings: setFloat requires (string, number) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    float value = static_cast<float>(args[1].ToNumber());
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    XPLMSetDataf(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatad(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber()) {
        LogMsg("JSBindings: setDouble requires (string, number) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    double value = args[1].ToNumber();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    XPLMSetDatad(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatavi(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsArray()) {
        LogMsg("JSBindings: setIntArray requires (string, array) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    // Convert JS array to int array
    JSArray arr = args[1].ToArray();
    std::vector<int> values;
    for (unsigned i = 0; i < arr.length(); i++) {
        values.push_back(static_cast<int>(arr[i].ToNumber()));
    }
    
    XPLMSetDatavi(ref, values.data(), offset, static_cast<int>(values.size()));
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatavf(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsArray()) {
        LogMsg("JSBindings: setFloatArray requires (string, array) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    // Convert JS array to float array
    JSArray arr = args[1].ToArray();
    std::vector<float> values;
    for (unsigned i = 0; i < arr.length(); i++) {
        values.push_back(static_cast<float>(arr[i].ToNumber()));
    }
    
    XPLMSetDatavf(ref, values.data(), offset, static_cast<int>(values.size()));
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatab(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsString()) {
        LogMsg("JSBindings: setData requires (string, string) arguments");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    String value = args[1].ToString();
    std::string value_str = value.utf8().data();
    
    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref) {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }
    
    if (!XPLMCanWriteDataRef(ref)) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    XPLMSetDatab(ref, const_cast<char*>(value_str.c_str()), offset, static_cast<int>(value_str.length()));
    return JSValue(true);
}

// =========================================================================
// Scenery API - Object Loading
// =========================================================================

JSValue JSBindings::JS_LoadObject(const JSObject& thisObject, const JSArgs& args) {
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
            // Return the path as the "handle" - we use path-based lookup
            return JSValue(path.utf8().data());
        }
    }
    
    // Load the object
    XPLMObjectRef obj = XPLMLoadObject(path_str.c_str());
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

JSValue JSBindings::JS_UnloadObject(const JSObject& thisObject, const JSArgs& args) {
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
    
    XPLMUnloadObject(it->second);
    object_cache_.erase(it);
    
    LogMsg("JSBindings: unloaded object: %s", path_str.c_str());
    return JSValue(true);
}

// =========================================================================
// Scenery API - Terrain Probing
// =========================================================================

JSValue JSBindings::JS_CreateProbe(const JSObject& thisObject, const JSArgs& args) {
    // Optional probe type argument (default to Y probe)
    int probeType = xplm_ProbeY;
    if (!args.empty() && args[0].IsNumber()) {
        probeType = static_cast<int>(args[0].ToNumber());
    }
    
    XPLMProbeRef probe = XPLMCreateProbe(static_cast<XPLMProbeType>(probeType));
    if (!probe) {
        LogMsg("JSBindings: failed to create terrain probe");
        return JSValue();
    }
    
    int id = next_probe_id_++;
    probe_cache_[id] = probe;
    
    LogMsg("JSBindings: created terrain probe with ID %d", id);
    return JSValue(id);
}

JSValue JSBindings::JS_DestroyProbe(const JSObject& thisObject, const JSArgs& args) {
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
    
    XPLMDestroyProbe(it->second);
    probe_cache_.erase(it);
    
    LogMsg("JSBindings: destroyed probe %d", id);
    return JSValue(true);
}

JSValue JSBindings::JS_ProbeTerrainXYZ(const JSObject& thisObject, const JSArgs& args) {
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
    
    XPLMProbeResult result = XPLMProbeTerrainXYZ(it->second, 
        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), &info);
    
    if (result != xplm_ProbeHitTerrain) {
        // Return result code so caller knows what happened
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
// Scenery API - Magnetic Variation
// =========================================================================

JSValue JSBindings::JS_GetMagneticVariation(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber()) {
        LogMsg("JSBindings: getMagneticVariation requires (latitude, longitude) arguments");
        return JSValue(0.0);
    }
    
    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();
    
    float variation = XPLMGetMagneticVariation(latitude, longitude);
    return JSValue(static_cast<double>(variation));
}

JSValue JSBindings::JS_DegTrueToDegMagnetic(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: degTrueToMagnetic requires a heading argument");
        return JSValue(0.0);
    }
    
    float headingTrue = static_cast<float>(args[0].ToNumber());
    float headingMag = XPLMDegTrueToDegMagnetic(headingTrue);
    return JSValue(static_cast<double>(headingMag));
}

JSValue JSBindings::JS_DegMagneticToDegTrue(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: degMagneticToTrue requires a heading argument");
        return JSValue(0.0);
    }
    
    float headingMag = static_cast<float>(args[0].ToNumber());
    float headingTrue = XPLMDegMagneticToDegTrue(headingMag);
    return JSValue(static_cast<double>(headingTrue));
}

// =========================================================================
// Instance API - Object Instancing
// =========================================================================

JSValue JSBindings::JS_CreateInstance(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: createInstance requires (objectPath, [datarefs]) arguments");
        return JSValue();
    }
    
    String path = args[0].ToString();
    std::string path_str = path.utf8().data();
    
    // Look up the object
    XPLMObjectRef obj = nullptr;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = object_cache_.find(path_str);
        if (it == object_cache_.end()) {
            LogMsg("JSBindings: object not loaded: %s", path_str.c_str());
            return JSValue();
        }
        obj = it->second;
    }
    
    // Parse dataref names array (optional)
    std::vector<std::string> dataref_strs;
    std::vector<const char*> datarefs;
    
    if (args.size() > 1 && args[1].IsArray()) {
        JSArray arr = args[1].ToArray();
        for (unsigned i = 0; i < arr.length(); i++) {
            if (arr[i].IsString()) {
                String s = arr[i].ToString();
                dataref_strs.push_back(s.utf8().data());
            }
        }
        
        // Build C string array (must be null-terminated)
        for (const auto& s : dataref_strs) {
            datarefs.push_back(s.c_str());
        }
        datarefs.push_back(nullptr);
    } else {
        // No datarefs - still need null terminator
        datarefs.push_back(nullptr);
    }
    
    XPLMInstanceRef instance = XPLMCreateInstance(obj, datarefs.data());
    if (!instance) {
        LogMsg("JSBindings: failed to create instance of: %s", path_str.c_str());
        return JSValue();
    }
    
    int id = next_instance_id_++;
    instance_cache_[id] = instance;
    
    LogMsg("JSBindings: created instance %d of object: %s", id, path_str.c_str());
    return JSValue(id);
}

JSValue JSBindings::JS_DestroyInstance(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: destroyInstance requires an instance ID argument");
        return JSValue(false);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = instance_cache_.find(id);
    if (it == instance_cache_.end()) {
        LogMsg("JSBindings: instance not found: %d", id);
        return JSValue(false);
    }
    
    XPLMDestroyInstance(it->second);
    instance_cache_.erase(it);
    
    LogMsg("JSBindings: destroyed instance %d", id);
    return JSValue(true);
}

JSValue JSBindings::JS_InstanceSetPosition(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsObject()) {
        LogMsg("JSBindings: instanceSetPosition requires (instanceId, position, [data]) arguments");
        return JSValue(false);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = instance_cache_.find(id);
    if (it == instance_cache_.end()) {
        LogMsg("JSBindings: instance not found: %d", id);
        return JSValue(false);
    }
    
    // Parse position object
    JSObject pos = args[1].ToObject();
    XPLMDrawInfo_t drawInfo;
    
    // Required position fields
    drawInfo.structSize = sizeof(XPLMDrawInfo_t);
    drawInfo.x = static_cast<float>(pos["x"].ToNumber());
    drawInfo.y = static_cast<float>(pos["y"].ToNumber());
    drawInfo.z = static_cast<float>(pos["z"].ToNumber());
    
    // Optional rotation fields (default to 0)
    drawInfo.pitch = pos["pitch"].IsNumber() ? static_cast<float>(pos["pitch"].ToNumber()) : 0.0f;
    drawInfo.heading = pos["heading"].IsNumber() ? static_cast<float>(pos["heading"].ToNumber()) : 0.0f;
    drawInfo.roll = pos["roll"].IsNumber() ? static_cast<float>(pos["roll"].ToNumber()) : 0.0f;
    
    // Parse data array (optional - for animated datarefs)
    std::vector<float> data;
    if (args.size() > 2 && args[2].IsArray()) {
        JSArray arr = args[2].ToArray();
        for (unsigned i = 0; i < arr.length(); i++) {
            data.push_back(static_cast<float>(arr[i].ToNumber()));
        }
    }
    
    XPLMInstanceSetPosition(it->second, &drawInfo, data.empty() ? nullptr : data.data());
    return JSValue(true);
}

// =========================================================================
// Graphics API - Coordinate Conversion
// =========================================================================

JSValue JSBindings::JS_LocalToWorld(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        LogMsg("JSBindings: localToWorld requires (x, y, z) arguments");
        return JSValue();
    }
    
    double x = args[0].ToNumber();
    double y = args[1].ToNumber();
    double z = args[2].ToNumber();
    
    double latitude, longitude, altitude;
    XPLMLocalToWorld(x, y, z, &latitude, &longitude, &altitude);
    
    JSObject result;
    result["latitude"] = JSValue(latitude);
    result["longitude"] = JSValue(longitude);
    result["altitude"] = JSValue(altitude);
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_WorldToLocal(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        LogMsg("JSBindings: worldToLocal requires (latitude, longitude, altitude) arguments");
        return JSValue();
    }
    
    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();
    double altitude = args[2].ToNumber();
    
    double x, y, z;
    XPLMWorldToLocal(latitude, longitude, altitude, &x, &y, &z);
    
    JSObject result;
    result["x"] = JSValue(x);
    result["y"] = JSValue(y);
    result["z"] = JSValue(z);
    
    return JSValue(static_cast<JSObjectRef>(result));
}
