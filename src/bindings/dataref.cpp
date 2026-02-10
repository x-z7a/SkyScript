#include "dataref.h"
#include "xplm_dispatch.h"

// Static member definitions
std::unordered_map<std::string, XPLMDataRef> DataRefBindings::dataref_cache_;
std::mutex DataRefBindings::cache_mutex_;

XPLMDataRef DataRefBindings::GetCachedDataRef(const std::string& name) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = dataref_cache_.find(name);
    if (it != dataref_cache_.end()) {
        return it->second;
    }
    
    XPLMDataRef ref = CallOnMainThread([&] { return XPLMFindDataRef(name.c_str()); });
    if (ref) {
        dataref_cache_[name] = ref;
    }
    return ref;
}

void DataRefBindings::Bind(JSObject& dataref) {
    // Lookup
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
}

// =========================================================================
// DataRef Lookup Functions
// =========================================================================

JSValue DataRefBindings::JS_FindDataRef(const JSObject& thisObject, const JSArgs& args) {
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

JSValue DataRefBindings::JS_CanWriteDataRef(const JSObject& thisObject, const JSArgs& args) {
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
    
    return JSValue(CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; }));
}

JSValue DataRefBindings::JS_GetDataRefTypes(const JSObject& thisObject, const JSArgs& args) {
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
    
    XPLMDataTypeID types = CallOnMainThread([&] { return XPLMGetDataRefTypes(ref); });
    
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

JSValue DataRefBindings::JS_GetDatai(const JSObject& thisObject, const JSArgs& args) {
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
    
    return JSValue(CallOnMainThread([&] { return XPLMGetDatai(ref); }));
}

JSValue DataRefBindings::JS_GetDataf(const JSObject& thisObject, const JSArgs& args) {
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
    
    return JSValue(static_cast<double>(CallOnMainThread([&] { return XPLMGetDataf(ref); })));
}

JSValue DataRefBindings::JS_GetDatad(const JSObject& thisObject, const JSArgs& args) {
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
    
    return JSValue(CallOnMainThread([&] { return XPLMGetDatad(ref); }));
}

JSValue DataRefBindings::JS_GetDatavi(const JSObject& thisObject, const JSArgs& args) {
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
    
    int size = CallOnMainThread([&] { return XPLMGetDatavi(ref, nullptr, 0, 0); });
    if (size <= 0) {
        return JSValue();
    }
    
    int offset = 0;
    int count = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        count = static_cast<int>(args[2].ToNumber());
    }
    
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue();
    if (count > size - offset) count = size - offset;
    
    std::vector<int> values(count);
    CallOnMainThread([&] { XPLMGetDatavi(ref, values.data(), offset, count); });
    
    JSArray result;
    for (int i = 0; i < count; i++) {
        result.push(JSValue(values[i]));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue DataRefBindings::JS_GetDatavf(const JSObject& thisObject, const JSArgs& args) {
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
    
    int size = CallOnMainThread([&] { return XPLMGetDatavf(ref, nullptr, 0, 0); });
    if (size <= 0) {
        return JSValue();
    }
    
    int offset = 0;
    int count = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        count = static_cast<int>(args[2].ToNumber());
    }
    
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue();
    if (count > size - offset) count = size - offset;
    
    std::vector<float> values(count);
    CallOnMainThread([&] { XPLMGetDatavf(ref, values.data(), offset, count); });
    
    JSArray result;
    for (int i = 0; i < count; i++) {
        result.push(JSValue(static_cast<double>(values[i])));
    }
    
    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue DataRefBindings::JS_GetDatab(const JSObject& thisObject, const JSArgs& args) {
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
    
    int size = CallOnMainThread([&] { return XPLMGetDatab(ref, nullptr, 0, 0); });
    if (size <= 0) {
        return JSValue("");
    }
    
    int offset = 0;
    int maxBytes = size;
    
    if (args.size() > 1 && args[1].IsNumber()) {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber()) {
        maxBytes = static_cast<int>(args[2].ToNumber());
    }
    
    if (offset < 0) offset = 0;
    if (offset >= size) return JSValue("");
    if (maxBytes > size - offset) maxBytes = size - offset;
    
    std::vector<char> buffer(maxBytes + 1, 0);
    CallOnMainThread([&] { XPLMGetDatab(ref, buffer.data(), offset, maxBytes); });
    
    return JSValue(buffer.data());
}

// =========================================================================
// Data Setters
// =========================================================================

JSValue DataRefBindings::JS_SetDatai(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    CallOnMainThread([&] { XPLMSetDatai(ref, value); });
    return JSValue(true);
}

JSValue DataRefBindings::JS_SetDataf(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    CallOnMainThread([&] { XPLMSetDataf(ref, value); });
    return JSValue(true);
}

JSValue DataRefBindings::JS_SetDatad(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    CallOnMainThread([&] { XPLMSetDatad(ref, value); });
    return JSValue(true);
}

JSValue DataRefBindings::JS_SetDatavi(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    JSArray arr = args[1].ToArray();
    std::vector<int> values;
    for (unsigned i = 0; i < arr.length(); i++) {
        values.push_back(static_cast<int>(arr[i].ToNumber()));
    }
    
    CallOnMainThread([&] { XPLMSetDatavi(ref, values.data(), offset, static_cast<int>(values.size())); });
    return JSValue(true);
}

JSValue DataRefBindings::JS_SetDatavf(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    JSArray arr = args[1].ToArray();
    std::vector<float> values;
    for (unsigned i = 0; i < arr.length(); i++) {
        values.push_back(static_cast<float>(arr[i].ToNumber()));
    }
    
    CallOnMainThread([&] { XPLMSetDatavf(ref, values.data(), offset, static_cast<int>(values.size())); });
    return JSValue(true);
}

JSValue DataRefBindings::JS_SetDatab(const JSObject& thisObject, const JSArgs& args) {
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
    
    if (!CallOnMainThread([&] { return XPLMCanWriteDataRef(ref) != 0; })) {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }
    
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber()) {
        offset = static_cast<int>(args[2].ToNumber());
    }
    
    CallOnMainThread([&] { XPLMSetDatab(ref, const_cast<char*>(value_str.c_str()), offset, static_cast<int>(value_str.length())); });
    return JSValue(true);
}
