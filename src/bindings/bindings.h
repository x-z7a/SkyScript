#pragma once

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/JSHelpers.h>

#include <string>

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
 *   - FsBindings        (fs.h)
 *   - PathBindings      (path.h)
 */
class JSBindings {
public:
    /**
     * @brief Bind all X-Plane API functions to a JavaScript context
     * @param view         The Ultralight view to bind functions to
     * @param app_name     Internal app name
     * @param app_display_name  Human-readable app name
     * @param app_dir      Absolute path to the app's root directory
     */
    static void BindToView(RefPtr<View> view,
                           const std::string& app_name,
                           const std::string& app_display_name,
                           const std::string& app_dir);
};
