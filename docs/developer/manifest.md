# App Manifest

Every SkyScript app requires a `manifest.yaml` file in its root folder. This file defines metadata and behavior for the app.

## Example

```yaml
name: My App
hide_addressbar: true
notification_corner: bottom-right
```

## Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | string | folder name | Display name shown in the Plugins menu. |
| `url` | string | — | URL to load. If omitted, SkyScript loads the local `index.html`. Alias: `homepage`. |
| `hide_addressbar` | boolean | `false` | Hide the built-in address bar. Recommended for apps that provide their own navigation. |
| `minimum_width` | integer | `0` | Minimum window width in pixels. The window never goes below 640. |
| `user_agent` | string | *(default Chrome UA)* | Value reported by `navigator.userAgent`. X-Plane sends its own `User-Agent` header. |
| `width` | integer | `1024` | Initial window width in pixels. |
| `height` | integer | `768` | Initial window height in pixels. |
| `console_logging` | boolean | `true` | Forward `console.log`/`warn`/`error` to X-Plane's `Log.txt`. Set to `false` for noisy apps. |
| `notification_corner` | string | `top-right` | Default notification location. Values: `top-left`, `top-right`, `bottom-left`, `bottom-right`. Alias: `notification_location`. |
| `notification_timeout` | float | `5` | Default auto-dismiss timeout in seconds. Use `0` to keep notifications visible until dismissed. Alias: `notification_timeout_seconds`. |
| `notification_opacity` | float | `0.78` | Default notification panel opacity. |
| `notification_slide_seconds` | float | `0.25` | Default notification slide-in and slide-out duration. |
| `notification_sound` | boolean | `true` | Play the bundled notification sound by default. |
| `default` | boolean | `false` | Default apps appear after a separator in the menu. Used for built-in apps like "About SkyScript". |

Local apps are loaded from their folder as a `file://` URL. Build them with relative asset paths (Create React App: `"homepage": "."`) and classic scripts: Chromium does not load ES modules or `fetch()` other files from a `file://` page. Apps that need either should be served from a URL instead.

## Window chrome

Every app window uses X-Plane's own title bar, close button and edge resizing. Since SDK 4.4.0 X-Plane draws browser windows itself, so there is no chrome for SkyScript to draw; the minimum size is 640×480.

## Removed options

These keys are still accepted so existing manifests keep working, but have no effect: X-Plane owns the browser and offers no way to apply them.

| Field | Was |
|-------|-----|
| `audio_muted` | Mute the page's audio |
| `framerate` | CEF rendering frame rate |
| `scroll_speed` | Scroll sensitivity multiplier |
| `forced_language` | Browser `Accept-Language` |
| `window_titleless` | SkyScript-drawn title bar and resize grip |
| `window_opacity` | Opacity of the titleless window |

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
