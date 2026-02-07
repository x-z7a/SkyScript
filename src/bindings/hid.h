#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "log_msg.h"
#include "../third_party/hidapi/hidapi.h"

#include <unordered_map>
#include <string>
#include <vector>

using namespace ultralight;

/**
 * @brief JavaScript bindings for HIDAPI (USB Human Interface Device access)
 *
 * Provides device enumeration, open/close, read/write, and feature reports.
 */
class HidBindings {
public:
    /**
     * @brief Bind HID functions to the given JS namespace object
     * @param hid The JS object to attach functions to
     */
    static void Bind(JSObject& hid);

    /**
     * @brief Close open HID handles and deinitialize HIDAPI.
     */
    static void Shutdown();

private:
    // HID device handle storage - maps device ID to handle
    static std::unordered_map<int, hid_device*> hid_device_cache_;
    static int next_hid_device_id_;
    static bool hid_initialized_;

    // Ensure HIDAPI is initialized (lazy init on first use)
    static void EnsureHidInitialized();

    static JSValue JS_HidEnumerate(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidOpen(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidOpenPath(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidClose(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidWrite(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidRead(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidSendFeatureReport(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidGetFeatureReport(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidGetDeviceInfo(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_HidSetNonBlocking(const JSObject& thisObject, const JSArgs& args);
};
