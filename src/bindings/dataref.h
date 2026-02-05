#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMDataAccess.h"
#include "log_msg.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane SDK DataAccess API
 *
 * Provides JavaScript bindings for the X-Plane DataRef system,
 * allowing web-based plugins to read and write simulator data.
 */
class DataRefBindings {
public:
    /**
     * @brief Bind DataRef functions to the given JS namespace object
     * @param dataref The JS object to attach functions to
     */
    static void Bind(JSObject& dataref);

    /**
     * @brief Get or cache a dataref handle by name
     */
    static XPLMDataRef GetCachedDataRef(const std::string& name);

private:
    // DataRef handle cache - maps dataref name to handle
    static std::unordered_map<std::string, XPLMDataRef> dataref_cache_;
    static std::mutex cache_mutex_;

    // =========================================================================
    // DataRef Lookup Functions
    // =========================================================================
    static JSValue JS_FindDataRef(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_CanWriteDataRef(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDataRefTypes(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Data Getters
    // =========================================================================
    static JSValue JS_GetDatai(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDataf(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDatad(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDatavi(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDatavf(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_GetDatab(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Data Setters
    // =========================================================================
    static JSValue JS_SetDatai(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_SetDataf(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_SetDatad(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_SetDatavi(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_SetDatavf(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_SetDatab(const JSObject& thisObject, const JSArgs& args);
};
