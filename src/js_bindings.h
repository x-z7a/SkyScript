#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMDataAccess.h"
#include "XPLMScenery.h"
#include "XPLMInstance.h"
#include "XPLMGraphics.h"
#include "log_msg.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

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

    // =========================================================================
    // Scenery API - Object Loading
    // =========================================================================
    
    // Object handle cache - maps path to handle
    static std::unordered_map<std::string, XPLMObjectRef> object_cache_;
    
    // Instance handle storage - maps instance ID to handle
    static std::unordered_map<int, XPLMInstanceRef> instance_cache_;
    static int next_instance_id_;
    
    // Probe handle storage - maps probe ID to handle
    static std::unordered_map<int, XPLMProbeRef> probe_cache_;
    static int next_probe_id_;

    /**
     * @brief Load an OBJ file
     * @param path Path to the .obj file relative to X-System folder
     * @return Object handle ID or null if failed
     */
    static JSValue JS_LoadObject(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Unload a previously loaded object
     * @param objectId The object handle ID
     * @return true if successful
     */
    static JSValue JS_UnloadObject(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Scenery API - Terrain Probing
    // =========================================================================

    /**
     * @brief Create a terrain probe
     * @return Probe handle ID
     */
    static JSValue JS_CreateProbe(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Destroy a terrain probe
     * @param probeId The probe handle ID
     */
    static JSValue JS_DestroyProbe(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Probe terrain at XYZ location
     * @param probeId The probe handle ID
     * @param x X coordinate (local OpenGL)
     * @param y Y coordinate (local OpenGL)
     * @param z Z coordinate (local OpenGL)
     * @return Object with terrain info or null if missed
     */
    static JSValue JS_ProbeTerrainXYZ(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Scenery API - Magnetic Variation
    // =========================================================================

    /**
     * @brief Get magnetic variation at a location
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @return Magnetic variation in degrees
     */
    static JSValue JS_GetMagneticVariation(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Convert true heading to magnetic
     * @param headingTrue True heading in degrees
     * @return Magnetic heading in degrees
     */
    static JSValue JS_DegTrueToDegMagnetic(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Convert magnetic heading to true
     * @param headingMagnetic Magnetic heading in degrees
     * @return True heading in degrees
     */
    static JSValue JS_DegMagneticToDegTrue(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Instance API - Object Instancing
    // =========================================================================

    /**
     * @brief Create an instance of a loaded object
     * @param objectId The object handle ID
     * @param datarefs Array of dataref names for animation
     * @return Instance handle ID or null if failed
     */
    static JSValue JS_CreateInstance(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Destroy an instance
     * @param instanceId The instance handle ID
     */
    static JSValue JS_DestroyInstance(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Set instance position and dataref values
     * @param instanceId The instance handle ID
     * @param position Object with x, y, z, pitch, heading, roll
     * @param data Array of float values for datarefs (must match order from creation)
     */
    static JSValue JS_InstanceSetPosition(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Graphics API - Coordinate Conversion
    // =========================================================================

    /**
     * @brief Convert local OpenGL coordinates to latitude/longitude/altitude
     * @param x Local X coordinate
     * @param y Local Y coordinate
     * @param z Local Z coordinate
     * @return Object with latitude, longitude, altitude
     */
    static JSValue JS_LocalToWorld(const JSObject& thisObject, const JSArgs& args);

    /**
     * @brief Convert latitude/longitude/altitude to local OpenGL coordinates
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param altitude Altitude in meters MSL
     * @return Object with x, y, z
     */
    static JSValue JS_WorldToLocal(const JSObject& thisObject, const JSArgs& args);
};
