# Project Structure

Once you've scaffolded a CRA app (see [Quick Start](./quick-start)), your project has several files that SkyScript cares about.

## Folder Layout

After `npm run build`, SkyScript expects each app folder under `plugins/SkyScript/apps/` to contain:

```
my-xplane-app/
├── index.html            # Entry point (required)
├── manifest.json         # App metadata (optional but recommended)
├── static/
│   ├── css/
│   └── js/
└── ...
```

SkyScript discovers apps by scanning for `index.html` in each subfolder of `apps/`.

## App Manifest

Create React App generates `public/manifest.json` automatically. SkyScript reads this file from your built app to configure the window:

| Field | Used for | Default |
|-------|----------|---------|
| `short_name` | Menu label and window title | Folder name |
| `width` | Initial window width (px) | `800` |
| `height` | Initial window height (px) | `600` |

Example `public/manifest.json`:

```json
{
  "short_name": "My App",
  "name": "My X-Plane App",
  "icons": [],
  "start_url": ".",
  "display": "standalone",
  "theme_color": "#000000",
  "background_color": "#ffffff",
  "width": 960,
  "height": 600
}
```

The `short_name` appears in the **Plugins → SkyScript** menu and in the window title bar. If omitted, the folder name is used instead.

## TypeScript Setup

To get autocomplete and type checking for the `XPlane` and `SkyScript` globals, add the type definitions to your project:

```bash
mkdir -p src/types/xplane
```

Copy [`index.d.ts`](https://github.com/x-z7a/SkyScript/blob/main/docs/api/types/xplane/index.d.ts) into `src/types/xplane/`, then update `tsconfig.json`:

```json
{
  "compilerOptions": {
    "typeRoots": ["./node_modules/@types", "./src/types"]
  }
}
```

This gives you full IntelliSense for calls like `XPlane.dataref.getFloat(...)`.

## Development Workflow

### Browser Preview

During development you can run the app in a normal browser:

```bash
npm start
```

The X-Plane API won't be available, so guard your calls:

```typescript
if (typeof XPlane !== 'undefined') {
  setAltitude(XPlane.dataref.getFloat("sim/flightmodel/position/elevation"));
} else {
  setAltitude(35000); // mock data
}
```

### Hot Reload in X-Plane

After building (`npm run build`) and copying to the apps folder, you can reload without restarting X-Plane:

```typescript
SkyScript.reloadApp('my-xplane-app');
```

Or use the App Manager UI (**Plugins → SkyScript → app_manager**) and click **Restart**.

## Multiple Apps

You can install as many apps as you like — each gets its own menu entry and window:

```
apps/
├── my-efb/
├── my-checklist/
└── hello-world/
```

Each app runs in its own Ultralight view with an isolated JavaScript context. They share the same plugin process but cannot directly communicate with each other.
