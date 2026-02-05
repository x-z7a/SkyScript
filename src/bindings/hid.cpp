#include "hid.h"

// Static member definitions
std::unordered_map<int, hid_device*> HidBindings::hid_device_cache_;
int HidBindings::next_hid_device_id_ = 1;
bool HidBindings::hid_initialized_ = false;

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

void HidBindings::EnsureHidInitialized() {
    if (!hid_initialized_) {
        if (hid_init() == 0) {
            hid_initialized_ = true;
            LogMsg("JSBindings: HIDAPI initialized");
        } else {
            LogMsg("JSBindings: HIDAPI initialization failed");
        }
    }
}

void HidBindings::Bind(JSObject& hid) {
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
}

JSValue HidBindings::JS_HidEnumerate(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidOpen(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber()) {
        LogMsg("JSBindings: hid.open requires (vendorId, productId) arguments");
        return JSValue();
    }
    
    EnsureHidInitialized();
    
    unsigned short vendor_id = static_cast<unsigned short>(args[0].ToNumber());
    unsigned short product_id = static_cast<unsigned short>(args[1].ToNumber());
    
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

JSValue HidBindings::JS_HidOpenPath(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidClose(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidWrite(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidRead(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidSendFeatureReport(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidGetFeatureReport(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidGetDeviceInfo(const JSObject& thisObject, const JSArgs& args) {
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

JSValue HidBindings::JS_HidSetNonBlocking(const JSObject& thisObject, const JSArgs& args) {
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
