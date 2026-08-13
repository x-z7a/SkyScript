# C API Reference

SkyScript exposes its functionality through a stable C API in `skyscript_c.h`. All functions use `extern "C"` linkage and plain C types, so they can be called from any language or toolchain (MSVC, MinGW, GCC, Clang, Go, etc.).

The C++ namespace API (`skyscript.h`) is still available for consumers that build from source and link directly, but the **C API is the recommended interface** for downstream plugins using the shared library distribution.

## Lifecycle

### `skyscript_initialize()`

```c
void skyscript_initialize(void);
```

Call from `XPluginStart`. Sets up:
- Plugin paths
- Mouse cursor resources
- Flight loop callback (handles app updates, keyboard focus sync)
- VR mode monitoring (auto-switches windows between desktop and VR)

### `skyscript_set_assets_path()`

```c
void skyscript_set_assets_path(const char* path);
```

Set the base directory for SkyScript assets (icons, sounds, etc.). If not called, defaults to `<pluginDirectory>/assets` where `pluginDirectory` is the SkyScript plugin folder.

Call after `skyscript_initialize()` and before creating any app windows. This is essential for **third-party plugins** that use SkyScript as a library, since the default path points to the SkyScript plugin directory which may not contain the assets.

**Example:**

```c
skyscript_initialize();

// Point to the assets/ folder bundled with your plugin
char pluginPath[512];
XPLMGetPluginInfo(XPLMGetMyID(), NULL, pluginPath, NULL, NULL);
char* lastSlash = strrchr(pluginPath, '/');
if (lastSlash) *lastSlash = '\0';
strcat(pluginPath, "/assets");
skyscript_set_assets_path(pluginPath);
```

The `assets/` directory is included in the SkyScript library distribution zip. Copy it into your plugin folder and point `skyscript_set_assets_path()` to it.

### `skyscript_shutdown()`

```c
void skyscript_shutdown(void);
```

Call from `XPluginStop`. Unregisters the flight loop, destroys all app windows, and frees cursor resources.

## App Discovery

### `skyscript_load_apps_from_directory()`

```c
int skyscript_load_apps_from_directory(void);
```

Scans the `apps/` directory inside the plugin folder. Each subfolder with a `manifest.yaml` is registered as an app window. Also creates the `skyscript/toggle` command for showing/hiding the last active app.

Returns `1` if at least one app was discovered, `0` otherwise.

### `skyscript_reload_apps()`

```c
void skyscript_reload_apps(void);
```

Destroys all current app windows and rescans the `apps/` directory. Use this to pick up newly installed or removed apps at runtime.

## Window Management

### `skyscript_create_app_window()`

```c
SkyScriptApp skyscript_create_app_window(
    const char* name,
    const char* id,
    const SkyScriptAppConfig* config
);
```

Create a browser window manually (outside of `skyscript_load_apps_from_directory`). The window is created hidden — call `skyscript_app_show()` to display it.

| Parameter | Description |
|-----------|-------------|
| `name` | Display name for the app |
| `id` | Unique identifier (used in dataref/command paths) |
| `config` | Window configuration, or `NULL` for defaults |

### `skyscript_default_config()`

```c
SkyScriptAppConfig skyscript_default_config(void);
```

Returns a default-initialized `SkyScriptAppConfig` struct.

### `skyscript_destroy_app_window()`

```c
void skyscript_destroy_app_window(SkyScriptApp app);
```

Destroy a specific browser window and free its resources. Removes it from the managed list.

### `skyscript_destroy_all_app_windows()`

```c
void skyscript_destroy_all_app_windows(void);
```

Destroy all managed windows and their associated dataref bindings.

### `skyscript_get_app_window_count()`

```c
int skyscript_get_app_window_count(void);
```

Returns the number of currently managed app windows.

### `skyscript_get_app_window_at()`

```c
SkyScriptApp skyscript_get_app_window_at(int index);
```

Returns the app window at the given index (0-based), or `NULL` if the index is out of range.

## Active App

### `skyscript_get_active_app()`

```c
SkyScriptApp skyscript_get_active_app(void);
```

Returns the most recently shown app, or `NULL` if none.

### `skyscript_set_active_app()`

```c
void skyscript_set_active_app(SkyScriptApp app);
```

Set the active app. Called automatically when an app is shown, but can be set manually.

### `skyscript_find_app()`

```c
SkyScriptApp skyscript_find_app(const char* id);
```

Find a managed app by its id (e.g. `"app-hello-world"`). Returns `NULL` if not found.

## App Accessors

### `skyscript_app_get_name()` / `skyscript_app_get_id()`

```c
const char* skyscript_app_get_name(SkyScriptApp app);
const char* skyscript_app_get_id(SkyScriptApp app);
```

Get the display name or unique identifier of an app. Returns `""` if `app` is `NULL`.

### `skyscript_app_is_visible()`

```c
int skyscript_app_is_visible(SkyScriptApp app);
```

Returns `1` if the app window is currently visible, `0` otherwise.

### `skyscript_app_show()` / `skyscript_app_hide()`

```c
void skyscript_app_show(SkyScriptApp app, const char* url);
void skyscript_app_hide(SkyScriptApp app);
```

Show or hide the app window. Pass `NULL` for `url` to show the current page.

## Notifications

SkyScript exposes native toast notifications for app windows. A notification slides in from a configurable corner, renders as a translucent panel, and can auto-dismiss after a timeout.

### `skyscript_default_notification_options()`

```c
SkyScriptNotificationOptions skyscript_default_notification_options(void);
```

Returns default notification options:

| Field | Default | Description |
|-------|---------|-------------|
| `title` | `""` | Optional heading text. |
| `body` | `""` | Optional body text. |
| `corner` | `SKYSCRIPT_NOTIFICATION_TOP_RIGHT` | Slide-in corner. |
| `timeout_seconds` | `5` | Seconds before auto-dismiss. Use `0` to keep it visible until dismissed. |
| `opacity` | `0.78` | Panel opacity, clamped between `0.15` and `0.95`. |
| `slide_seconds` | `0.25` | Slide-in and slide-out duration. |
| `dismissible` | `1` | Show a close affordance. |
| `play_sound` | `1` | Play the bundled notification sound. |

### `skyscript_app_show_notification()`

```c
void skyscript_app_show_notification(
    SkyScriptApp app,
    const SkyScriptNotificationOptions* options
);
```

Show a notification in the given app window. Showing a new notification replaces the current one.

```c
SkyScriptNotificationOptions options = skyscript_default_notification_options();
options.title = "Route imported";
options.body = "KSFO to KSEA is ready.";
options.corner = SKYSCRIPT_NOTIFICATION_BOTTOM_RIGHT;
options.timeout_seconds = 4.0f;

skyscript_app_show_notification(app, &options);
```

### `skyscript_app_dismiss_notification()`

```c
void skyscript_app_dismiss_notification(SkyScriptApp app);
```

Dismiss the current notification with the configured slide-out animation.

### C++ and Go

The C++ API exposes `SkyScript::showNotification(app, options)` and `SkyScript::dismissNotification(app)`. Go bindings expose `app.Notify(title, body)`, `app.ShowNotification(options)`, and `app.DismissNotification()`.

## Message Passing

SkyScript provides a bidirectional message channel between the native plugin and JavaScript running in app windows. This enables plugins to expose custom functions and push structured data to the JS frontend.

### `skyscript_app_on_message()`

```c
typedef void (*SkyScriptMessageCallback)(
    const char* channel,
    const char* payload,
    char** out_response,  // set to malloc'd string on success
    char** out_error,     // set to malloc'd string on failure
    void* user_data
);

void skyscript_app_on_message(
    SkyScriptApp app,
    const char* channel,
    SkyScriptMessageCallback callback,
    void* user_data
);
```

Register a handler for messages sent from JavaScript via `window.skyscript.postMessage(channel, payload)`.

The handler receives the JSON payload string and must set either `*out_response` or `*out_error`:
- Set `*out_response` to a `malloc`'d JSON string on success (SkyScript frees it).
- Set `*out_error` to a `malloc`'d error string on failure (SkyScript frees it).

The handler runs on the simulator's main thread and blocks that frame until it
returns, so it must not wait on anything slow. If the answer comes from a
socket, a file, or another process, return a receipt immediately and deliver the
result later with [`skyscript_app_post_message()`](#skyscript_app_post_message),
correlating the two with an id of your own.

**Example:**

```c
void on_get_profile(const char* channel, const char* payload,
                    char** out_response, char** out_error, void* user_data) {
    *out_response = strdup("{\"name\":\"default\",\"version\":1}");
}

skyscript_app_on_message(app, "getProfile", on_get_profile, NULL);
```

### `skyscript_app_post_message()`

```c
void skyscript_app_post_message(SkyScriptApp app, const char* channel, const char* payload);
```

Push a message from the plugin to JavaScript. All JS listeners registered via `window.skyscript.onMessage(channel, callback)` will be called with the parsed payload.

**Threading:** safe to call from any thread. The message is queued and delivered
on the next flight loop, so a worker thread never touches CEF directly.

**Payload:** must be a JSON document. It is parsed in the page with
`JSON.parse`, not evaluated. A payload that is not valid JSON is dropped with a
`console.error` rather than executed.

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

## Logging

### `skyscript_set_log_prefix()`

```c
void skyscript_set_log_prefix(const char* prefix);
```

Set the log prefix used in debug messages (e.g. `"[MyPlugin]"`).

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

See `example/main.cpp` in the repository for a complete X-Plane plugin that uses the SkyScript C API to build menus, manage app visibility, and check for updates.
