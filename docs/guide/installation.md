# Installation

## Download

Download the latest release from the [GitHub Releases](https://github.com/x-z7a/skyscript-cef/releases) page. Choose the `.zip` file that matches your X-Plane version (e.g. `SkyScript-1.0.6-XP12.zip`).

## Install the Plugin

1. Locate your X-Plane installation folder.
2. Navigate to `Resources/plugins/`.
3. Extract the downloaded zip so that the **SkyScript** folder is placed directly inside `plugins/`:

```
X-Plane 12/
└── Resources/
    └── plugins/
        └── SkyScript/
            ├── mac_x64/
            │   └── SkyScript.xpl
            ├── win_x64/
            │   └── SkyScript.xpl
            ├── lin_x64/
            │   └── SkyScript.xpl
            ├── apps/
            └── assets/
```

::: tip
Only the `.xpl` file for your operating system needs to be present. You can safely delete the other platform folders.
:::

## Verify

1. Launch X-Plane 12.
2. Load any aircraft.
3. Open the menu: **Plugins > SkyScript**. You should see a list of installed apps.

## Updating

Replace the `SkyScript` folder with the contents of the new release zip. Your installed apps inside `apps/` will be preserved as long as you merge rather than overwrite the folder.

SkyScript also supports the [Skunkcrafts Updater](https://forums.x-plane.org/index.php?/forums/topic/144828-skunkcrafts-updater-v40/) for automatic updates.
