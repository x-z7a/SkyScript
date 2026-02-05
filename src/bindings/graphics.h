#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include "XPLMGraphics.h"
#include "log_msg.h"

using namespace ultralight;

/**
 * @brief JavaScript bindings for X-Plane Graphics API
 *
 * Provides coordinate conversion between local OpenGL and world coordinates.
 */
class GraphicsBindings {
public:
    /**
     * @brief Bind Graphics functions to the given JS namespace object
     * @param graphics The JS object to attach functions to
     */
    static void Bind(JSObject& graphics);

private:
    static JSValue JS_LocalToWorld(const JSObject& thisObject, const JSArgs& args);
    static JSValue JS_WorldToLocal(const JSObject& thisObject, const JSArgs& args);
};
