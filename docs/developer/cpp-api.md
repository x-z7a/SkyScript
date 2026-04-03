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

## Registered Datarefs & Commands

SkyScript automatically registers these X-Plane datarefs and commands:

| ID | Type | Description |
|----|------|-------------|
| `skyscript/toggle` | Command | Show/hide the last active app |
| `skyscript/app-{id}/toggle` | Command | Show/hide a specific app |
| `skyscript/app-{id}/refresh` | Command | Reload the page |
| `skyscript/app-{id}/visible` | Dataref (int) | `1` when visible |
| `skyscript/app-{id}/url` | Dataref (string) | Current URL (writable) |

## Example

See `example/main.cpp` in the repository for a complete X-Plane plugin that uses SkyScript to build menus, manage app visibility, and check for updates.
