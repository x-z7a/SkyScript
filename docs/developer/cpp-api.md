# C++ Library API

SkyScript exposes its functionality through a single header: `skyscript.h`. All functions live in the `SkyScript` namespace.

## Lifecycle

### `initialize()`

```cpp
void SkyScript::initialize();
```

Call from `XPluginStart`. Sets up:
- Plugin paths
- Mouse cursor resources
- Flight loop callback (handles app updates, keyboard focus sync)
- VR mode monitoring (auto-switches windows between desktop and VR)

### `setAssetsPath()`

```cpp
void SkyScript::setAssetsPath(const std::string& path);
```

Set the base directory for SkyScript assets (icons, sounds, etc.). If not called, defaults to `<pluginDirectory>/assets` where `pluginDirectory` is the SkyScript plugin folder.

Call after `initialize()` and before creating any app windows. This is essential for **third-party plugins** that use SkyScript as a library, since the default path points to the SkyScript plugin directory which may not contain the assets.

**Example:**

```cpp
SkyScript::initialize();

// Point to the assets/ folder bundled with your plugin
char pluginPath[512];
XPLMGetPluginInfo(XPLMGetMyID(), nullptr, pluginPath, nullptr, nullptr);
std::string myPluginDir = std::string(pluginPath).substr(0, std::string(pluginPath).rfind('/'));
SkyScript::setAssetsPath(myPluginDir + "/assets");
```

The `assets/` directory is included in the SkyScript library distribution zip. Copy it into your plugin folder and point `setAssetsPath()` to it.

### `shutdown()`

```cpp
void SkyScript::shutdown();
```

Call from `XPluginStop`. Unregisters the flight loop, destroys all app windows, and frees cursor resources.

## App Discovery

### `loadAppsFromDirectory()`

```cpp
bool SkyScript::loadAppsFromDirectory();
```

Scans the `apps/` directory inside the plugin folder. Each subfolder with a `manifest.yaml` is registered as an app window. Also creates the `skyscript/toggle` command for showing/hiding the last active app.

Returns `true` if at least one app was discovered.

### `reloadApps()`

```cpp
void SkyScript::reloadApps();
```

Destroys all current app windows and rescans the `apps/` directory. Use this to pick up newly installed or removed apps at runtime.

## Window Management

### `createAppWindow()`

```cpp
App* SkyScript::createAppWindow(
    const std::string& name,
    const std::string& id,
    const AppConfiguration& config = App::defaultConfig()
);
```

Create a browser window manually (outside of `loadAppsFromDirectory`). The window is created hidden — call `app->showBrowser()` to display it.

| Parameter | Description |
|-----------|-------------|
| `name` | Display name for the app |
| `id` | Unique identifier (used in dataref/command paths) |
| `config` | Window configuration parsed from a manifest, or `App::defaultConfig()` |

### `destroyAppWindow()`

```cpp
void SkyScript::destroyAppWindow(App* app);
```

Destroy a specific browser window and free its resources. Removes it from the managed list.

### `destroyAllAppWindows()`

```cpp
void SkyScript::destroyAllAppWindows();
```

Destroy all managed windows and their associated dataref bindings.

### `getAppWindows()`

```cpp
const std::vector<App*>& SkyScript::getAppWindows();
```

Returns all currently managed app windows. Regular apps are listed before default apps.

## Active App

### `getActiveApp()`

```cpp
App* SkyScript::getActiveApp();
```

Returns the most recently shown app, or `nullptr` if none.

### `setActiveApp()`

```cpp
void SkyScript::setActiveApp(App* app);
```

Set the active app. Called automatically when an app is shown, but can be set manually.

### `findApp()`

```cpp
App* SkyScript::findApp(const std::string& id);
```

Find a managed app by its id (e.g. `"app-hello-world"`). Returns `nullptr` if not found.

## Message Passing

SkyScript provides a bidirectional message channel between the native plugin and JavaScript running in app windows. This enables plugins to expose custom functions and push structured data to the JS frontend.

### `app->onMessage()`

```cpp
app->onMessage(
    const std::string& channel,
    std::function<std::pair<std::string, std::string>(const std::string& payload)> handler
);
```

Register a handler for messages sent from JavaScript via `window.skyscript.postMessage(channel, payload)`.

The handler receives the JSON payload string and must return a `pair<response, error>`:
- If `error` is non-empty, the JS Promise is rejected with the error message.
- If `error` is empty, the JS Promise is resolved with the `response` string (must be valid JSON).

| Parameter | Description |
|-----------|-------------|
| `channel` | Channel name to handle (e.g. `"getProfile"`) |
| `handler` | Callback that processes the request and returns `{response, error}` |

**Example:**

```cpp
App* app = SkyScript::findApp("app-my-plugin");

app->onMessage("getProfile", [](const std::string& payload) -> std::pair<std::string, std::string> {
    // payload is the JSON string sent from JS
    // Return {jsonResponse, ""} on success, or {"", errorMessage} on failure
    return {R"({"name":"default","version":1})", ""};
});

app->onMessage("saveProfile", [](const std::string& payload) -> std::pair<std::string, std::string> {
    // Parse and save the profile...
    return {"null", ""};  // success with no meaningful return value
});
```

### `app->postMessageToJS()`

```cpp
app->postMessageToJS(const std::string& channel, const std::string& payload);
```

Push a message from the plugin to JavaScript. All JS listeners registered via `window.skyscript.onMessage(channel, callback)` will be called with the parsed payload.

| Parameter | Description |
|-----------|-------------|
| `channel` | Channel name to send on |
| `payload` | JSON string to send (will be parsed in JS) |

**Example:**

```cpp
App* app = SkyScript::findApp("app-my-plugin");

// Notify JS of a profile update
app->postMessageToJS("profileUpdated", R"({"name":"default","version":2})");

// Send validation results
app->postMessageToJS("validationResult", R"({"errors":[],"warnings":["minor issue"]})");
```

### Go Bindings

```go
// Register a handler for messages from JS
app.OnMessage("getProfile", func(payload string) (string, error) {
    // payload is the JSON string from JS
    return `{"name":"default"}`, nil
})

// Push data to JS
app.PostMessage("profileUpdated", `{"name":"default","version":2}`)
```

### C API

```c
// Callback signature
typedef void (*SkyScriptMessageCallback)(
    const char* channel,
    const char* payload,
    char** out_response,  // set to malloc'd string on success
    char** out_error,     // set to malloc'd string on failure
    void* user_data
);

void skyscript_app_on_message(SkyScriptApp app, const char* channel,
                              SkyScriptMessageCallback callback, void* user_data);
void skyscript_app_post_message(SkyScriptApp app, const char* channel, const char* payload);
```

## Registered Datarefs & Commands

SkyScript automatically registers these X-Plane datarefs and commands:

| ID | Type | Description |
|----|------|-------------|
| `skyscript/toggle` | Command | Show/hide the last active app |
| `skyscript/app-{id}/toggle` | Command | Show/hide a specific app |
| `skyscript/app-{id}/refresh` | Command | Reload the page |
| `skyscript/app-{id}/devtools` | Command | Toggle Chrome DevTools window for debugging |
| `skyscript/app-{id}/visible` | Dataref (int) | `1` when visible |
| `skyscript/app-{id}/url` | Dataref (string) | Current URL (writable) |

## Example

See `example/main.cpp` in the repository for a complete X-Plane plugin that uses SkyScript to build menus, manage app visibility, and check for updates.
