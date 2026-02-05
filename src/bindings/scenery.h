#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMScenery.h"
#include "log_msg.h"

#include <unordered_map>
#include <string>
#include <mutex>

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane Scenery API
 *
 * Provides object loading, terrain probing, and magnetic variation functions.
 */
class SceneryBindings {
public:
    /**
     * @brief Bind Scenery functions to the given JS namespace object
     * @param scenery The JS object to attach functions to
     */
    static void Bind(JSObject& scenery);

private:
    // Object handle cache - maps path to handle
    static std::unordered_map<std::string, XPLMObjectRef> object_cache_;
    static std::mutex cache_mutex_;

    // Probe handle storage - maps probe ID to handle
    static std::unordered_map<int, XPLMProbeRef> probe_cache_;
    static int next_probe_id_;

    // Allow InstanceBindings to access the object cache
    friend class InstanceBindings;

    // =========================================================================
    // Object Loading
    // =========================================================================
    static JSValue JS_LoadObject(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_UnloadObject(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Terrain Probing
    // =========================================================================
    static JSValue JS_CreateProbe(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_DestroyProbe(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_ProbeTerrainXYZ(const JSObject& thisObject, const JSArgs& args);

    // =========================================================================
    // Magnetic Variation
    // =========================================================================
    static JSValue JS_GetMagneticVariation(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_DegTrueToDegMagnetic(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_DegMagneticToDegTrue(const JSObject& thisObject, const JSArgs& args);
};
