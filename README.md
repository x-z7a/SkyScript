# SkyScript

SkyScript is a **reusable C++ library** for rendering web content (HTML/React apps) inside X-Plane 12 floating windows using CEF (Chromium Embedded Framework). It provides a JavaScript API bridge for apps to interact with X-Plane datarefs and supports both desktop and VR modes.

The library handles browser lifecycle, flight loop updates, VR mode switching, app discovery, and keyboard focus — your plugin just calls `initialize()` and `shutdown()`.

## Architecture

```
src/              → SkyScriptLib (static library)
example/          → Example X-Plane plugin using the library
apps/             → React/HTML apps loaded at runtime
lib/<platform>/   → Pre-built CEF binaries
```

## Requirements

- X-Plane SDK 4.2.0
- CEF tree under `lib/<platform>/cef`
- CMake 3.25.1+, C++23

## Build

```sh
./build_platforms.sh mac
./build_platforms.sh --platforms mac,win --clean
./build_platforms.sh --sdk-root /path/to/SDK mac
```

## Library API

```cpp
#include "skyscript.h"

// In XPluginStart:
SkyScript::initialize();       // Registers flight loop, VR monitoring, cursor

// When aircraft loads:
SkyScript::loadAppsFromDirectory();  // Scans apps/ folder, creates windows

// In XPluginStop:
SkyScript::shutdown();         // Unregisters flight loop, destroys all windows
```

Additional API:

| Function | Description |
|----------|-------------|
| `reloadApps()` | Tear down and rescan the apps/ directory |
| `createAppWindow(name, id, config)` | Create a browser window manually |
| `destroyAppWindow(app)` | Destroy a specific window |
| `destroyAllAppWindows()` | Destroy all managed windows |
| `getAppWindows()` | Get all managed app windows |
| `getActiveApp()` / `setActiveApp(app)` | Active (most recently shown) app |
| `findApp(id)` | Find an app by its id |

## Runtime Dataref/Command

- `skyscript/toggle` — toggle visibility of the last active app
- `skyscript/app-{folder}/toggle` — toggle a specific app
- `skyscript/app-{folder}/visible` — get/set visibility
- `skyscript/app-{folder}/url` — current URL (writable)
- `skyscript/app-{folder}/refresh` — reload the page
