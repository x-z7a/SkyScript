#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

using namespace ultralight;

/**
 * @brief Central coordinator for all JavaScript bindings
 *
 * Delegates to individual binding modules in src/bindings/:
 *   - DataRefBindings   (dataref.h)
 *   - SceneryBindings   (scenery.h)
 *   - InstanceBindings  (instance.h)
 *   - GraphicsBindings  (graphics.h)
 *   - SkyScriptBindings (skyscript.h)
 *   - HidBindings       (hid.h)
 */
class JSBindings {
public:
    /**
     * @brief Bind all X-Plane API functions to a JavaScript context
     * @param view The Ultralight view to bind functions to
     */
    static void BindToView(RefPtr<View> view);
};
