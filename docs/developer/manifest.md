# App Manifest

Every SkyScript app requires a `manifest.yaml` file in its root folder. This file defines metadata and behavior for the app.

## Example

```yaml
name: My App
hide_addressbar: true
framerate: 20
notification_corner: bottom-right
```

## Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | string | folder name | Display name shown in the Plugins menu. |
| `url` | string | — | URL to load. If omitted, SkyScript loads the local `index.html`. Alias: `homepage`. |
| `hide_addressbar` | boolean | `false` | Hide the built-in address bar. Recommended for apps that provide their own navigation. |
| `audio_muted` | boolean | `false` | Mute all audio from this app's browser. |
| `framerate` | integer | `25` | CEF rendering frame rate (FPS). Lower values reduce CPU usage. |
| `minimum_width` | integer | `0` | Minimum window width in pixels. `0` means no minimum. |
| `scroll_speed` | integer | `5` | Scroll sensitivity multiplier. |
| `user_agent` | string | *(default Chrome UA)* | Custom `User-Agent` header sent with every request. |
| `forced_language` | string | *(auto-detected)* | Override the browser language. Format: `en-US,en` or `fr-FR,fr`. |
| `width` | integer | `1024` | Initial window width in pixels. |
| `height` | integer | `768` | Initial window height in pixels. |
| `console_logging` | boolean | `true` | Forward `console.log`/`warn`/`error` to X-Plane's `Log.txt`. Set to `false` for noisy apps. |
| `window_titleless` | boolean | `true` | Use SkyScript's titleless self-decorated window instead of X-Plane's native title bar. |
| `window_opacity` | float | `0.96` | Browser texture opacity when `window_titleless` is enabled. Use `1` for a fully solid window. |
| `notification_corner` | string | `top-right` | Default notification location. Values: `top-left`, `top-right`, `bottom-left`, `bottom-right`. Alias: `notification_location`. |
| `notification_timeout` | float | `5` | Default auto-dismiss timeout in seconds. Use `0` to keep notifications visible until dismissed. Alias: `notification_timeout_seconds`. |
| `notification_opacity` | float | `0.78` | Default notification panel opacity. |
| `notification_slide_seconds` | float | `0.25` | Default notification slide-in and slide-out duration. |
| `notification_sound` | boolean | `true` | Play the bundled notification sound by default. |
| `default` | boolean | `false` | Default apps appear after a separator in the menu. Used for built-in apps like "About SkyScript". |

Local apps are served through an app-specific `https://*.skyscript.local/` origin at runtime. This keeps pure client apps compatible with modern browser features such as ES modules while still loading files from the app folder.

Titleless windows keep X-Plane resizing support and provide a slim draggable strip along the top edge. Apps that hide SkyScript's injected address bar get a slightly taller drag strip.

## Minimal Manifests

### Local app (with `index.html`)

```yaml
name: My App
```

### URL-only app (no build step)

```yaml
name: Navigraph Charts
url: https://charts.navigraph.com
hide_addressbar: true
```

A URL-only app needs only `manifest.yaml` — no `package.json`, no build step, no `index.html`.

## How Apps Are Discovered

On aircraft load (or when **Reload configuration** is triggered), SkyScript scans the `apps/` directory inside the plugin folder. Each subfolder containing a `manifest.yaml` is registered as an app.

The app's **ID** is derived from the folder name, prefixed with `app-`. For example:

| Folder | App ID |
|--------|--------|
| `apps/hello-world/` | `app-hello-world` |
| `apps/navigraph/` | `app-navigraph` |

This ID is used in dataref and command paths (e.g. `skyscript/app-hello-world/toggle`).
