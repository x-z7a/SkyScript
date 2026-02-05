#include "js_bindings.h"
#include "manager.h"

// Static member definitions
std::unordered_map<std::string, XPLMDataRef> JSBindings::dataref_cache_;
std::mutex JSBindings::cache_mutex_;

// Scenery/Instance static members
std::unordered_map<std::string, XPLMObjectRef> JSBindings::object_cache_;
std::unordered_map<int, XPLMInstanceRef> JSBindings::instance_cache_;
int JSBindings::next_instance_id_ = 1;
std::unordered_map<int, XPLMProbeRef> JSBindings::probe_cache_;
int JSBindings::next_probe_id_ = 1;

// HID static members
std::unordered_map<int, hid_device*> JSBindings::hid_device_cache_;
int JSBindings::next_hid_device_id_ = 1;
bool JSBindings::hid_initialized_ = false;

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
    
    // =========================================================================
    // Create the hid sub-namespace
    // =========================================================================
    JSObject hid;
    
    hid["enumerate"] = JSCallbackWithRetval(JS_HidEnumerate);
    hid["open"] = JSCallbackWithRetval(JS_HidOpen);
    hid["openPath"] = JSCallbackWithRetval(JS_HidOpenPath);
    hid["close"] = JSCallbackWithRetval(JS_HidClose);
    hid["write"] = JSCallbackWithRetval(JS_HidWrite);
    hid["read"] = JSCallbackWithRetval(JS_HidRead);
    hid["sendFeatureReport"] = JSCallbackWithRetval(JS_HidSendFeatureReport);
    hid["getFeatureReport"] = JSCallbackWithRetval(JS_HidGetFeatureReport);
    hid["getDeviceInfo"] = JSCallbackWithRetval(JS_HidGetDeviceInfo);
    hid["setNonBlocking"] = JSCallbackWithRetval(JS_HidSetNonBlocking);
    
    xplane["hid"] = JSValue(static_cast<JSObjectRef>(hid));
    
    // Attach XPlane to global
    global["XPlane"] = JSValue(static_cast<JSObjectRef>(xplane));
    
    // =========================================================================
    // Create the SkyScript namespace (for app management)
    // =========================================================================
    JSObject skyscript;
    
    skyscript["listApps"] = JSCallbackWithRetval(JS_ListApps);
    skyscript["reloadApp"] = JSCallbackWithRetval(JS_ReloadApp);
    skyscript["openAppWindow"] = JSCallbackWithRetval(JS_OpenAppWindow);
    skyscript["openAppInspector"] = JSCallbackWithRetval(JS_OpenAppInspector);
    
    // Attach SkyScript to global
    global["SkyScript"] = JSValue(static_cast<JSObjectRef>(skyscript));
    
    LogMsg("JSBindings: Bound XPlane API (dataref, scenery, instance, graphics, hid) and SkyScript API to view");
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

// =========================================================================
// SkyScript App Management API
// =========================================================================

JSValue JSBindings::JS_ListApps(const JSObject& thisObject, const JSArgs& args) {
    auto names = Manager::instance().getAppNames();
    
    JSArray result;
    for (size_t i = 0; i < names.size(); i++) {
        result.push(JSValue(names[i].c_str()));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_ReloadApp(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: reloadApp requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(Manager::instance().reloadApp(name_str));
}

JSValue JSBindings::JS_OpenAppWindow(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: openAppWindow requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(Manager::instance().openAppWindow(name_str));
}

JSValue JSBindings::JS_OpenAppInspector(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: openAppInspector requires a string argument (app name)");
        return JSValue(false);
    }
    
    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    
    return JSValue(Manager::instance().openAppInspector(name_str));
}

// =========================================================================
// HID API - USB Human Interface Device access
// =========================================================================

namespace {
    // Helper to convert wchar_t* to std::string (UTF-8)
    std::string WcharToString(const wchar_t* wstr) {
        if (!wstr) return "";
        std::string result;
        for (const wchar_t* p = wstr; *p; ++p) {
            wchar_t c = *p;
            if (c < 0x80) {
                result.push_back(static_cast<char>(c));
            } else if (c < 0x800) {
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            } else if (c < 0x10000) {
                result.push_back(static_cast<char>(0xE0 | (c >> 12)));
                result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
        return result;
    }
}

void JSBindings::EnsureHidInitialized() {
    if (!hid_initialized_) {
        if (hid_init() == 0) {
            hid_initialized_ = true;
            LogMsg("JSBindings: HIDAPI initialized");
        } else {
            LogMsg("JSBindings: HIDAPI initialization failed");
        }
    }
}

JSValue JSBindings::JS_HidEnumerate(const JSObject& thisObject, const JSArgs& args) {
    EnsureHidInitialized();

    unsigned short vendor_id = 0;
    unsigned short product_id = 0;
    
    if (args.size() > 0 && args[0].IsNumber()) {
        vendor_id = static_cast<unsigned short>(args[0].ToNumber());
    }
    if (args.size() > 1 && args[1].IsNumber()) {
        product_id = static_cast<unsigned short>(args[1].ToNumber());
    }
    
    struct hid_device_info *devs = hid_enumerate(vendor_id, product_id);
    
    JSArray result;
    struct hid_device_info *cur = devs;
    while (cur) {
        JSObject dev;
        dev["path"] = JSValue(cur->path ? cur->path : "");
        dev["vendorId"] = JSValue(static_cast<int>(cur->vendor_id));
        dev["productId"] = JSValue(static_cast<int>(cur->product_id));
        dev["serialNumber"] = JSValue(WcharToString(cur->serial_number).c_str());
        dev["releaseNumber"] = JSValue(static_cast<int>(cur->release_number));
        dev["manufacturer"] = JSValue(WcharToString(cur->manufacturer_string).c_str());
        dev["product"] = JSValue(WcharToString(cur->product_string).c_str());
        dev["usagePage"] = JSValue(static_cast<int>(cur->usage_page));
        dev["usage"] = JSValue(static_cast<int>(cur->usage));
        dev["interfaceNumber"] = JSValue(cur->interface_number);
        
        result.push(JSValue(static_cast<JSObjectRef>(dev)));
        cur = cur->next;
    }
    
    hid_free_enumeration(devs);
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_HidOpen(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber()) {
        LogMsg("JSBindings: hid.open requires (vendorId, productId) arguments");
        return JSValue();
    }
    
    EnsureHidInitialized();
    
    unsigned short vendor_id = static_cast<unsigned short>(args[0].ToNumber());
    unsigned short product_id = static_cast<unsigned short>(args[1].ToNumber());
    
    // Optional serial number
    wchar_t *serial = nullptr;
    std::wstring serial_wstr;
    if (args.size() > 2 && args[2].IsString()) {
        String serial_str = args[2].ToString();
        std::string s = serial_str.utf8().data();
        serial_wstr.assign(s.begin(), s.end());
        serial = const_cast<wchar_t*>(serial_wstr.c_str());
    }
    
    hid_device *dev = hid_open(vendor_id, product_id, serial);
    if (!dev) {
        LogMsg("JSBindings: hid.open failed for VID=0x%04X PID=0x%04X", vendor_id, product_id);
        return JSValue();
    }
    
    int id = next_hid_device_id_++;
    hid_device_cache_[id] = dev;
    
    LogMsg("JSBindings: HID device opened (ID=%d, VID=0x%04X, PID=0x%04X)", id, vendor_id, product_id);
    return JSValue(id);
}

JSValue JSBindings::JS_HidOpenPath(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: hid.openPath requires a string path argument");
        return JSValue();
    }
    
    EnsureHidInitialized();
    
    String path = args[0].ToString();
    std::string path_str = path.utf8().data();
    
    hid_device *dev = hid_open_path(path_str.c_str());
    if (!dev) {
        LogMsg("JSBindings: hid.openPath failed for path: %s", path_str.c_str());
        return JSValue();
    }
    
    int id = next_hid_device_id_++;
    hid_device_cache_[id] = dev;
    
    LogMsg("JSBindings: HID device opened by path (ID=%d, path=%s)", id, path_str.c_str());
    return JSValue(id);
}

JSValue JSBindings::JS_HidClose(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: hid.close requires a device ID argument");
        return JSValue(false);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue(false);
    }
    
    hid_close(it->second);
    hid_device_cache_.erase(it);
    
    LogMsg("JSBindings: HID device closed (ID=%d)", id);
    return JSValue(true);
}

JSValue JSBindings::JS_HidWrite(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsArray()) {
        LogMsg("JSBindings: hid.write requires (deviceId, data[]) arguments");
        return JSValue(-1);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue(-1);
    }
    
    JSArray arr = args[1].ToArray();
    std::vector<unsigned char> data;
    for (unsigned i = 0; i < arr.length(); i++) {
        data.push_back(static_cast<unsigned char>(arr[i].ToNumber()));
    }
    
    int result = hid_write(it->second, data.data(), data.size());
    return JSValue(result);
}

JSValue JSBindings::JS_HidRead(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber()) {
        LogMsg("JSBindings: hid.read requires (deviceId, length) arguments");
        return JSValue();
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    int length = static_cast<int>(args[1].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue();
    }
    
    if (length <= 0 || length > 65535) {
        LogMsg("JSBindings: hid.read invalid length: %d", length);
        return JSValue();
    }
    
    std::vector<unsigned char> buf(length);
    int bytes_read;
    
    if (args.size() > 2 && args[2].IsNumber()) {
        int timeout_ms = static_cast<int>(args[2].ToNumber());
        bytes_read = hid_read_timeout(it->second, buf.data(), buf.size(), timeout_ms);
    } else {
        bytes_read = hid_read(it->second, buf.data(), buf.size());
    }
    
    if (bytes_read < 0) {
        return JSValue();
    }
    
    JSArray result;
    for (int i = 0; i < bytes_read; i++) {
        result.push(JSValue(static_cast<int>(buf[i])));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_HidSendFeatureReport(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsArray()) {
        LogMsg("JSBindings: hid.sendFeatureReport requires (deviceId, data[]) arguments");
        return JSValue(-1);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue(-1);
    }
    
    JSArray arr = args[1].ToArray();
    std::vector<unsigned char> data;
    for (unsigned i = 0; i < arr.length(); i++) {
        data.push_back(static_cast<unsigned char>(arr[i].ToNumber()));
    }
    
    int result = hid_send_feature_report(it->second, data.data(), data.size());
    return JSValue(result);
}

JSValue JSBindings::JS_HidGetFeatureReport(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        LogMsg("JSBindings: hid.getFeatureReport requires (deviceId, reportId, length) arguments");
        return JSValue();
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    int report_id = static_cast<int>(args[1].ToNumber());
    int length = static_cast<int>(args[2].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue();
    }
    
    if (length <= 0 || length > 65535) {
        return JSValue();
    }
    
    std::vector<unsigned char> buf(length);
    buf[0] = static_cast<unsigned char>(report_id);
    
    int bytes_read = hid_get_feature_report(it->second, buf.data(), buf.size());
    if (bytes_read < 0) {
        return JSValue();
    }
    
    JSArray result;
    for (int i = 0; i < bytes_read; i++) {
        result.push(JSValue(static_cast<int>(buf[i])));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_HidGetDeviceInfo(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsNumber()) {
        LogMsg("JSBindings: hid.getDeviceInfo requires a device ID argument");
        return JSValue();
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue();
    }
    
    wchar_t buf[256];
    JSObject info;
    
    if (hid_get_manufacturer_string(it->second, buf, 256) == 0) {
        info["manufacturer"] = JSValue(WcharToString(buf).c_str());
    } else {
        info["manufacturer"] = JSValue("");
    }
    
    if (hid_get_product_string(it->second, buf, 256) == 0) {
        info["product"] = JSValue(WcharToString(buf).c_str());
    } else {
        info["product"] = JSValue("");
    }
    
    if (hid_get_serial_number_string(it->second, buf, 256) == 0) {
        info["serialNumber"] = JSValue(WcharToString(buf).c_str());
    } else {
        info["serialNumber"] = JSValue("");
    }
    
    return JSValue(static_cast<JSObjectRef>(info));
}

JSValue JSBindings::JS_HidSetNonBlocking(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsBoolean()) {
        LogMsg("JSBindings: hid.setNonBlocking requires (deviceId, nonBlocking) arguments");
        return JSValue(false);
    }
    
    int id = static_cast<int>(args[0].ToNumber());
    bool non_blocking = args[1].ToBoolean();
    
    auto it = hid_device_cache_.find(id);
    if (it == hid_device_cache_.end()) {
        LogMsg("JSBindings: HID device not found: %d", id);
        return JSValue(false);
    }
    
    int result = hid_set_nonblocking(it->second, non_blocking ? 1 : 0);
    return JSValue(result == 0);
}
