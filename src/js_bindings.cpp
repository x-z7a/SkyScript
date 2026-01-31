#include "js_bindings.h"

// Static member definitions
std::unordered_map<std::string, XPLMDataRef> JSBindings::dataref_cache_;
std::mutex JSBindings::cache_mutex_;

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
    
    // Attach XPlane to global
    global["XPlane"] = JSValue(static_cast<JSObjectRef>(xplane));
    
    LogMsg("JSBindings: Bound XPlane.dataref API to view");
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
