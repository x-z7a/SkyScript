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
| `window_titleless` | boolean | `true` | Use SkyScript's title bar and resize grip instead of X-Plane's title bar. See [Window chrome](#window-chrome). |
| `window_opacity` | float | `0.96` | Page opacity for titleless windows, `0.2`–`1`. Use `1` for a fully solid window. |
| `notification_corner` | string | `top-right` | Default notification location. Values: `top-left`, `top-right`, `bottom-left`, `bottom-right`. Alias: `notification_location`. |
| `notification_timeout` | float | `5` | Default auto-dismiss timeout in seconds. Use `0` to keep notifications visible until dismissed. Alias: `notification_timeout_seconds`. |
| `notification_opacity` | float | `0.78` | Default notification panel opacity. |
| `notification_slide_seconds` | float | `0.25` | Default notification slide-in and slide-out duration. |
| `notification_sound` | boolean | `true` | Play the bundled notification sound by default. |
| `default` | boolean | `false` | Default apps appear after a separator in the menu. Used for built-in apps like "About SkyScript". |

Local apps are loaded from their folder as a `file://` URL. Build them with relative asset paths (Create React App: `"homepage": "."`) and classic scripts: Chromium does not load ES modules or `fetch()` other files from a `file://` page. Apps that need either should be served from a URL instead.

## Window chrome

With `window_titleless` (the default), SkyScript frames the window itself:

| Part | Height | Contains |
|------|--------|----------|
| Title bar | 28px | The close button, the app `name`, and the drag region |
| Page | `height` | Your app |
| Footer | 16px | The resize grip, at the right |

X-Plane draws browser windows itself and nothing can be drawn into them, so the title bar and footer are two small windows of SkyScript's own, glued to the page's top and bottom edges. **They sit outside the page:** `width` and `height` are the size of your app, and nothing SkyScript draws covers your content.

- Drag the title bar to move the window. The close button hides it.
- Drag the grip to resize, anchored at the top-left corner. X-Plane also resizes the window from its edges. Both clamp to a 640×480 page (or `minimum_width`).
- `window_opacity` is applied to the page as CSS (`html { opacity }`), because X-Plane has no opacity setting for browser windows. The bars use it too.
- In VR the chrome is hidden and X-Plane frames the window.

With `window_titleless: false`, X-Plane draws its own title bar and owns resizing, `width`/`height` are the window size, and `window_opacity` is ignored.

## Removed options

These keys are still accepted so existing manifests keep working, but have no effect: X-Plane owns the browser and offers no way to apply them.

| Field | Was |
|-------|-----|
| `audio_muted` | Mute the page's audio |
| `framerate` | CEF rendering frame rate |
| `scroll_speed` | Scroll sensitivity multiplier |
| `forced_language` | Browser `Accept-Language` |

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
