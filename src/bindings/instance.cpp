#include "instance.h"
#include "scenery.h"
#include "xplm_dispatch.h"

// Static member definitions
std::unordered_map<int, XPLMInstanceRef> InstanceBindings::instance_cache_;
int InstanceBindings::next_instance_id_ = 1;

void InstanceBindings::Bind(JSObject& instance) {
    instance["create"] = JSCallbackWithRetval(JS_CreateInstance);
    instance["destroy"] = JSCallbackWithRetval(JS_DestroyInstance);
    instance["setPosition"] = JSCallbackWithRetval(JS_InstanceSetPosition);
}

JSValue InstanceBindings::JS_CreateInstance(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("JSBindings: createInstance requires (objectPath, [datarefs]) arguments");
        return JSValue();
    }
    
    String path = args[0].ToString();
    std::string path_str = path.utf8().data();
    
    // Look up the object in SceneryBindings' cache
    XPLMObjectRef obj = nullptr;
    {
        std::lock_guard<std::mutex> lock(SceneryBindings::cache_mutex_);
        auto it = SceneryBindings::object_cache_.find(path_str);
        if (it == SceneryBindings::object_cache_.end()) {
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
        
        for (const auto& s : dataref_strs) {
            datarefs.push_back(s.c_str());
        }
        datarefs.push_back(nullptr);
    } else {
        datarefs.push_back(nullptr);
    }
    
    XPLMInstanceRef instance = CallOnMainThread([&] { return XPLMCreateInstance(obj, datarefs.data()); });
    if (!instance) {
        LogMsg("JSBindings: failed to create instance of: %s", path_str.c_str());
        return JSValue();
    }
    
    int id = next_instance_id_++;
    instance_cache_[id] = instance;
    
    LogMsg("JSBindings: created instance %d of object: %s", id, path_str.c_str());
    return JSValue(id);
}

JSValue InstanceBindings::JS_DestroyInstance(const JSObject& thisObject, const JSArgs& args) {
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
    
    CallOnMainThread([&] { XPLMDestroyInstance(it->second); });
    instance_cache_.erase(it);
    
    LogMsg("JSBindings: destroyed instance %d", id);
    return JSValue(true);
}

JSValue InstanceBindings::JS_InstanceSetPosition(const JSObject& thisObject, const JSArgs& args) {
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
    
    drawInfo.structSize = sizeof(XPLMDrawInfo_t);
    drawInfo.x = static_cast<float>(pos["x"].ToNumber());
    drawInfo.y = static_cast<float>(pos["y"].ToNumber());
    drawInfo.z = static_cast<float>(pos["z"].ToNumber());
    
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
    
    CallOnMainThread([&] { XPLMInstanceSetPosition(it->second, &drawInfo, data.empty() ? nullptr : data.data()); });
    return JSValue(true);
}
