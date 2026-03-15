# Installing Apps

SkyScript apps are self-contained folders placed inside the plugin's `apps/` directory. The plugin discovers them automatically when an aircraft is loaded.

## Install an App

1. Download or build the app. You should have a folder containing at least a `manifest.yaml` file, and usually an `index.html` with supporting assets.
2. Copy the folder into your SkyScript `apps/` directory:

```
SkyScript/
└── apps/
    ├── my-new-app/        <-- paste here
    │   ├── manifest.yaml
    │   ├── index.html
    │   └── static/
    ├── about/
    └── web-browser/
```

3. In X-Plane, go to **Plugins > SkyScript > Reload configuration** to pick up the new app without restarting the sim.
4. The app now appears in **Plugins > SkyScript**.

## Uninstall an App

Delete the app's folder from `SkyScript/apps/` and reload configuration.

## App Types

### Local apps

Most apps ship a built `index.html` with CSS and JavaScript. SkyScript loads this file directly — no internet connection is needed for the app itself.

### URL-only apps

Some apps simply point to an external website (like Navigraph Charts). These contain only a `manifest.yaml` with a `url` field and require an internet connection.

## Datarefs and Commands

Each app automatically registers X-Plane datarefs and commands under the `skyscript/` namespace:

| ID | Type | Description |
|----|------|-------------|
| `skyscript/app-{folder}/toggle` | Command | Show or hide the app window |
| `skyscript/app-{folder}/refresh` | Command | Reload the current page |
| `skyscript/app-{folder}/visible` | Dataref (int) | `1` when the app window is visible |
| `skyscript/app-{folder}/url` | Dataref (string) | Current URL — writable to navigate programmatically |

For example, an app in the folder `apps/hello-world/` registers `skyscript/app-hello-world/toggle`.

You can bind these commands to joystick buttons or keyboard shortcuts using X-Plane's settings or third-party binding tools.
