# SkyScript

SkyScript is a **reusable C/C++ library** for rendering web content (HTML/React apps) inside X-Plane 12 floating windows using X-Plane's built-in browser windows (`xplm_WindowContentTypeBrowser`, SDK 4.4.0). It provides a JavaScript API bridge for apps to interact with X-Plane datarefs, native slide-out notifications, and both desktop and VR modes.

The library is distributed as a **shared library** (`.dll` on Windows, `.dylib` on macOS, `.so` on Linux) with a stable **C API**, allowing downstream plugins to link against it without C++ ABI coupling. This means plugins can be built with any toolchain — MSVC, MinGW, GCC, or Clang.

X-Plane renders the page and routes mouse and keyboard input to it; SkyScript handles app discovery, the `window.skyscript` JavaScript API, notifications, flight loop updates and VR mode switching — your plugin just calls `skyscript_initialize()` and `skyscript_shutdown()`.

## Architecture

```
src/              → SkyScriptLib (shared library)
src/skyscript_c.h → Public C API header
example/          → Example X-Plane plugin using the C API
apps/             → React/HTML apps loaded at runtime
```

## Requirements

- X-Plane 12.4.4 or newer at runtime
- X-Plane SDK 4.4.0 or newer to build
- CMake 3.25.1+, C++23

No CEF tree or runtime is needed: X-Plane owns the browser.

## License

SkyScript is licensed under the MIT License.

## Build

```sh
./build_platforms.sh mac
./build_platforms.sh --platforms mac,win --clean
./build_platforms.sh --sdk-root /path/to/SDK mac
```

## Library API (C)

```c
#include "skyscript_c.h"

// In XPluginStart:
skyscript_initialize();       // Registers flight loop and VR monitoring

// When aircraft loads:
skyscript_load_apps_from_directory();  // Scans apps/ folder, creates windows

// In XPluginStop:
skyscript_shutdown();         // Unregisters flight loop, destroys all windows
```

Additional C API:

| Function | Description |
|----------|-------------|
| `skyscript_reload_apps()` | Tear down and rescan the apps/ directory |
| `skyscript_create_app_window(name, id, config)` | Create a browser window manually |
| `skyscript_destroy_app_window(app)` | Destroy a specific window |
| `skyscript_destroy_all_app_windows()` | Destroy all managed windows |
| `skyscript_get_app_window_count()` | Get number of managed app windows |
| `skyscript_get_app_window_at(index)` | Get app window by index |
| `skyscript_get_active_app()` / `skyscript_set_active_app(app)` | Active (most recently shown) app |
| `skyscript_find_app(id)` | Find an app by its id |
| `skyscript_app_show_notification(app, options)` | Show a native slide-out notification |
| `skyscript_app_dismiss_notification(app)` | Dismiss the current notification |

Web apps can use the injected notification hooks:

```js
await skyscript.notify('Route imported', 'KSFO to KSEA is ready.', {
  corner: 'bottom-right',
  timeoutSeconds: 4
});
```

## Runtime Dataref/Command

- `skyscript/toggle` — toggle visibility of the last active app
- `skyscript/app-{folder}/toggle` — toggle a specific app
- `skyscript/app-{folder}/visible` — get/set visibility
- `skyscript/app-{folder}/url` — current URL (writable)
- `skyscript/app-{folder}/refresh` — reload the page
