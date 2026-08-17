# Running Two SkyScript Plugins

Two SkyScript-based plugins in one copy of X-Plane need one packaging step, or
the second one loaded will quietly serve the first one's apps.

## The problem

Every consumer ships its own copy of the SkyScript library, and every copy
carries the same identity:

| Platform | Identity | Set by |
|----------|----------|--------|
| macOS | `@rpath/libSkyScriptLib.dylib` | `LC_ID_DYLIB` install name |
| Linux | `libSkyScriptLib.so` | `SONAME` |
| Windows | `SkyScriptLib.dll` | module base name |

Dynamic loaders key loaded images by that identity, **not by path**. So when
X-Plane loads a second SkyScript plugin, the loader sees a library it already
has and hands over the existing image rather than mapping a new one.

Both plugins then share one set of globals: the `Path` singleton holding the
plugin directory, the app registry, the `Dataref` bindings. The second plugin
scans the *first* plugin's `apps/` folder, adopts its windows, and never reads
its own manifest.

It does not look like a crash. It looks like a plugin that works, and shows the
wrong window:

```
[zoal-charts] Started (version 0.0.0-dev)
[zoal-charts]: Dataref is not owned by you - you cannot unregister.
[zoal-charts]: XPLMWindowCallbackRecord at 0x... is being deleted by a different plugin.
- 0x... previously created by zoal-atc v0.1.0 (ai.zoal.atc) at 0.00000
[zoal-atc skyscript] Discovered app: zoal-atc (id: app-zoal-atc, default: no)
[zoal-charts] Loaded 1 app(s)
```

The tell is the prefix: those `[zoal-atc skyscript]` lines were written *during*
zoal-charts' own `skyscript_load_apps_from_directory()` call.

Since this release, `skyscript_initialize()` detects a second consumer in the
same image and says so in `Log.txt` rather than leaving you to work it out.

## The fix

Give each plugin's copy an identity of its own, as the last step before zipping
a release:

```sh
scripts/unique-library-identity.sh dist/my-plugin my-plugin
```

`dist/my-plugin` is the folder that goes into `Resources/plugins` — the one
holding `mac_x64/`, `win_x64/` and `lin_x64/`. Platforms that are not there are
skipped, so packaging on one machine works.

What it does per platform:

- **macOS** — renames to `libSkyScriptLib-<plugin>.dylib`, rewrites the install
  name with `install_name_tool -id`, points each `.xpl` at it with
  `-change`, then re-signs both ad-hoc. The re-signing is required, not
  cosmetic: editing a Mach-O invalidates its signature and arm64 refuses to load
  a modified binary whose signature no longer matches.
- **Linux** — renames to `libSkyScriptLib-<plugin>.so`, sets the `SONAME` with
  `patchelf --set-soname`, and repoints each `.xpl` with
  `patchelf --replace-needed`. Needs `patchelf` installed.
- **Windows** — PE has no `install_name_tool`, and the DLL name lives in the
  import descriptor as a plain string, so it is patched in place. In place means
  same length: `SkyScriptLib.dll` is 16 characters and so is
  `SkyS` + 8 hex + `.dll`, so the name is derived from a hash of the plugin
  name. The file size is unchanged and the PE structure is untouched.

## What is still shared

Renaming stops plugins from sharing an *image*. X-Plane's command namespace is
still global, and this release namespaces what SkyScript owns in it:

- `skyscript/<plugin>/toggle` — every plugin gets one only it owns.
- `skyscript/toggle` — kept for anyone who already bound it, but claimed only by
  the first plugin to ask. A second plugin logs that it was taken and uses its
  namespaced command instead.

Per-app commands and datarefs were already safe: the app id comes from its
folder name, so `skyscript/app-charts/*` and `skyscript/app-zoal-atc/*` never
collided.
