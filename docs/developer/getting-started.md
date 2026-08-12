# Getting Started

SkyScript is a shared library that adds CEF browser windows to X-Plane 12 plugins. It exposes a stable C API so downstream plugins can link against it without C++ ABI coupling. This guide covers two topics:

1. **Integrating the library** into your X-Plane plugin
2. **Building a web app** that runs inside a SkyScript browser window

## Integrating the Library

### Prerequisites

- CMake 3.25.1+, C or C++ compiler (any toolchain: MSVC, MinGW, GCC, Clang)
- X-Plane SDK 4.2.0
- SkyScript shared library (`.dll`/`.dylib`/`.so`) from the library distribution

### Library Distribution Contents

The SkyScript library distribution zip contains:

```
SkyScript-lib/
├── include/
│   └── skyscript_c.h       # C API header (the only header you need)
├── lib/
│   ├── win_x64/
│   │   ├── SkyScriptLib.dll     # Windows shared library
│   │   ├── SkyScriptLib.lib     # Windows import library (for MSVC)
│   │   ├── cef/                 # CEF headers/libraries used by SkyScript
│   │   └── ...                  # CEF runtime files for win_x64
│   ├── mac_x64/
│   │   ├── libSkyScriptLib.dylib
│   │   ├── cef/                 # CEF framework and headers
│   │   └── ...                  # CEF runtime files for mac_x64
│   └── lin_x64/
│       ├── libSkyScriptLib.so
│       ├── cef/                 # CEF headers/libraries used by SkyScript
│       └── ...                  # CEF runtime files for lin_x64
├── go/                      # Go bindings
├── go.mod
├── assets/                  # Icons, sounds, etc.
└── LICENSE
```

### CMake Setup

Link your plugin against the SkyScript shared library:

```cmake
# Find the SkyScript shared library
find_library(SKYSCRIPT_LIBRARY NAMES SkyScriptLib PATHS "path/to/skyscript/lib/${PLATFORM}_x64" REQUIRED)

# Add your plugin
add_library(MyPlugin MODULE src/main.cpp)
target_link_libraries(MyPlugin PRIVATE ${SKYSCRIPT_LIBRARY})
target_include_directories(MyPlugin PRIVATE "path/to/skyscript/include")
```

If you are building from the SkyScript source tree via `add_subdirectory`:

```cmake
add_subdirectory(path/to/skyscript)
target_link_libraries(MyPlugin PRIVATE SkyScriptLib)
```

### Minimal Plugin

```c
#include "skyscript_c.h"

PLUGIN_API int XPluginStart(char* name, char* sig, char* desc) {
    strcpy(name, "My Plugin");
    strcpy(sig, "com.example.myplugin");
    strcpy(desc, "Plugin with embedded browser");

    skyscript_initialize();   // Registers flight loop, VR monitoring, cursor
    return 1;
}

PLUGIN_API void XPluginStop() {
    skyscript_shutdown();     // Unregisters flight loop, destroys all windows
}

PLUGIN_API int XPluginEnable() {
    skyscript_load_apps_from_directory();  // Scan apps/ folder
    return 1;
}

PLUGIN_API void XPluginDisable() {}
```

### Bundling Assets

The SkyScript library distribution includes an `assets/` folder containing icons, sounds, and other resources used by SkyScript UI elements (back buttons, notifications, etc.).

If you are building a **third-party plugin** that uses SkyScript as a library, you need to:

1. **Copy the `assets/` directory** from the library distribution into your plugin folder.
2. **Call `skyscript_set_assets_path()`** after `skyscript_initialize()` to tell SkyScript where to find the assets.

```c
#include "skyscript_c.h"
#include <XPLMPlugin.h>

PLUGIN_API int XPluginStart(char* name, char* sig, char* desc) {
    strcpy(name, "My Plugin");
    strcpy(sig, "com.example.myplugin");
    strcpy(desc, "Plugin with embedded browser");

    skyscript_initialize();

    // Point SkyScript to the assets bundled with your plugin
    char pluginPath[512];
    XPLMGetPluginInfo(XPLMGetMyID(), NULL, pluginPath, NULL, NULL);
    // Trim filename to get directory, then append /assets
    char* lastSlash = strrchr(pluginPath, '/');
    if (lastSlash) *lastSlash = '\0';
    strcat(pluginPath, "/assets");
    skyscript_set_assets_path(pluginPath);

    return 1;
}
```

Without this step, SkyScript will look for assets in its own plugin directory (`Resources/plugins/SkyScript/assets/`), which won't exist if SkyScript is only used as a library.

### Deploying the Shared Library

At runtime, the SkyScript shared library must be loadable by the operating system:

- **Windows**: Place `SkyScriptLib.dll` next to your `.xpl` plugin, or in the same `win_x64/` directory.
- **macOS**: Place `libSkyScriptLib.dylib` next to your `.xpl` plugin, or in the same `mac_x64/` directory.
- **Linux**: Place `libSkyScriptLib.so` next to your `.xpl` plugin, or in the same `lin_x64/` directory.

Also copy the platform CEF runtime files from the same distribution directory
beside the plugin. The shared library contains SkyScript, not Chromium itself.

For **Go bindings**, the equivalent calls are:

```go
skyscript.Initialize()
skyscript.SetAssetsPath(myPluginDir + "/assets")
```

See the [C API Reference](/developer/cpp-api) for the full reference and the `example/` folder in the repo for a complete working plugin.

---

## Building a Web App

This section walks you through creating a SkyScript app using the **Hello World** example as a reference.

### Prerequisites

- [Node.js](https://nodejs.org/) 18 or later
- A text editor

## Scaffold the Project

Create a new folder inside `apps/` in your SkyScript plugin directory (or in the repo if you're developing from source):

```bash
mkdir apps/my-app && cd apps/my-app
```

### Create the manifest

Every app needs a `manifest.yaml` at its root. Create `apps/my-app/manifest.yaml`:

```yaml
name: My App
hide_addressbar: true
framerate: 20
```

See the [Manifest Reference](/developer/manifest) for all available fields.

### Bootstrap with Create React App

The Hello World example uses React with TypeScript. You can use any framework — or plain HTML — but React is a good starting point:

```bash
npx create-react-app . --template typescript
```

Set the `homepage` field in `package.json` so the built assets use relative paths:

```json
{
  "homepage": "."
}
```

### Project structure

After scaffolding, your app should look like this:

```
apps/my-app/
├── manifest.yaml        # Required — app metadata
├── package.json
├── public/
│   └── index.html
├── src/
│   ├── index.tsx
│   ├── App.tsx
│   └── ...
└── tsconfig.json
```

## Add TypeScript Declarations

Create `src/react-app-env.d.ts` (or add to your existing one) so TypeScript knows about the SkyScript API:

```typescript
/// <reference types="react-scripts" />

interface SkyscriptXplm {
  getDataref(ref: string): Promise<number | string | number[]>;
  setDataref(ref: string, value: number | string, valueType?: string): Promise<void>;
  executeCommand(command: string): Promise<void>;
}

interface Window {
  skyscript: {
    xplm: SkyscriptXplm;
  };
}
```

## Write Your App

Replace `src/App.tsx` with a simple component that reads a dataref:

```tsx
import { useState } from 'react';

function App() {
  const [zuluTime, setZuluTime] = useState('--');

  async function readTime() {
    try {
      const value = await window.skyscript.xplm.getDataref(
        'sim/time/zulu_time_sec'
      );
      setZuluTime(Number(value).toFixed(1));
    } catch (err: any) {
      setZuluTime(`Error: ${err.message}`);
    }
  }

  return (
    <div style={{ padding: 40, textAlign: 'center', color: '#e0e0e0' }}>
      <h1>My App</h1>
      <p>Zulu time: {zuluTime}</p>
      <button onClick={readTime}>Read Dataref</button>
    </div>
  );
}

export default App;
```

## Build

```bash
npm run build
```

This produces a `build/` folder containing `index.html` and static assets. SkyScript serves this folder directly.

## Deploy to X-Plane

If you're developing outside the plugin directory, copy the built output:

```bash
cp -r build/ /path/to/X-Plane/Resources/plugins/SkyScript/apps/my-app/
cp manifest.yaml /path/to/X-Plane/Resources/plugins/SkyScript/apps/my-app/
```

If you're developing inside the repo's `apps/` folder, the build script handles this automatically:

```bash
# From the repo root
./build_platforms.sh mac
```

This compiles the plugin, builds all apps, and copies everything to the X-Plane plugins folder.

## Test

1. In X-Plane, go to **Plugins > SkyScript > Reload configuration**.
2. Open **Plugins > SkyScript > My App**.
3. Click **Read Dataref** — you should see the current Zulu time.

## Next Steps

- Read the full [JavaScript API Reference](/developer/api) to learn about `setDataref` and `executeCommand`.
- Read the [C++ Library API](/developer/cpp-api) for the full reference.
- Check the [Manifest Reference](/developer/manifest) for options like `user_agent`, `framerate`, and `audio_muted`.
- Look at the [Hello World source](https://github.com/x-z7a/skyscript/tree/main/apps/hello-world) for a more complete example with polling, custom dataref input, and command execution.
