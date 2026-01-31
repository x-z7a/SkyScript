#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMDataAccess.h"
#include "log_msg.h"

#include <unordered_map>
#include <string>
#include <mutex>

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane SDK DataAccess API
 * 
 * This class provides JavaScript bindings for the X-Plane DataRef system,
 * allowing web-based plugins to read and write simulator data.
 */
class JSBindings {
public:
    /**
     * @brief Bind all X-Plane API functions to a JavaScript context
     * @param view The Ultralight view to bind functions to
     */
    static void BindToView(RefPtr<View> view);

private:
    // DataRef handle cache - maps dataref name to handle
    static std::unordered_map<std::string, XPLMDataRef> dataref_cache_;
    static std::mutex cache_mutex_;

    // Helper to get or cache a dataref
    static XPLMDataRef GetCachedDataRef(const std::string& name);

    // =========================================================================
    // DataRef Lookup Functions
    // =========================================================================

    /**
     * @brief Find a dataref by name
     * @param name The dataref path (e.g., "sim/cockpit/radios/nav1_freq_hz")
     * @return Handle ID (number) or null if not found
     */
    static JSValue JS_FindDataRef(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Check if a dataref is writable
     * @param name The dataref path
     * @return true if writable, false otherwise
     */
    static JSValue JS_CanWriteDataRef(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get the type(s) of a dataref
     * @param name The dataref path
     * @return Object with boolean flags for each type
     */
    static JSValue JS_GetDataRefTypes(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Data Getters
    // =========================================================================

    /**
     * @brief Get an integer dataref value
     * @param name The dataref path
     * @return Integer value
     */
    static JSValue JS_GetDatai(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get a float dataref value
     * @param name The dataref path
     * @return Float value
     */
    static JSValue JS_GetDataf(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get a double dataref value
     * @param name The dataref path
     * @return Double value
     */
    static JSValue JS_GetDatad(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get an integer array dataref
     * @param name The dataref path
     * @param offset (optional) Start offset in array, default 0
     * @param count (optional) Number of elements to read, default all
     * @return Array of integers
     */
    static JSValue JS_GetDatavi(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get a float array dataref
     * @param name The dataref path
     * @param offset (optional) Start offset in array, default 0
     * @param count (optional) Number of elements to read, default all
     * @return Array of floats
     */
    static JSValue JS_GetDatavf(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Get a byte array (data) dataref as string
     * @param name The dataref path
     * @param offset (optional) Start offset, default 0
     * @param maxBytes (optional) Maximum bytes to read, default all
     * @return String value
     */
    static JSValue JS_GetDatab(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Data Setters
    // =========================================================================

    /**
     * @brief Set an integer dataref value
     * @param name The dataref path
     * @param value The value to set
     */
    static JSValue JS_SetDatai(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set a float dataref value
     * @param name The dataref path
     * @param value The value to set
     */
    static JSValue JS_SetDataf(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set a double dataref value
     * @param name The dataref path
     * @param value The value to set
     */
    static JSValue JS_SetDatad(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set an integer array dataref
     * @param name The dataref path
     * @param values Array of integers to write
     * @param offset (optional) Start offset in array, default 0
     */
    static JSValue JS_SetDatavi(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set a float array dataref
     * @param name The dataref path
     * @param values Array of floats to write
     * @param offset (optional) Start offset in array, default 0
     */
    static JSValue JS_SetDatavf(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set a byte array (data) dataref from string
     * @param name The dataref path
     * @param value String value to write
     * @param offset (optional) Start offset, default 0
     */
    static JSValue JS_SetDatab(const JSObject& thisObject, const JSArgs& args);
};
