#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMInstance.h"
#include "XPLMGraphics.h"
#include "log_msg.h"

#include <unordered_map>
#include <string>
#include <vector>

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane Instance API
 *
 * Provides object instancing: create, destroy, and position instances.
 */
class InstanceBindings {
public:
    /**
     * @brief Bind Instance functions to the given JS namespace object
     * @param instance The JS object to attach functions to
     */
    static void Bind(JSObject& instance);

private:
    // Instance handle storage - maps instance ID to handle
    static std::unordered_map<int, XPLMInstanceRef> instance_cache_;
    static int next_instance_id_;

    static JSValue JS_CreateInstance(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_DestroyInstance(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_InstanceSetPosition(const JSObject& thisObject, const JSArgs& args);
};
