#!/usr/bin/env bash
set -euo pipefail

# Give one plugin's copy of the SkyScript library an identity no other plugin
# shares, so two SkyScript-based plugins can run in the same sim.
#
# The problem this solves: every consumer ships the same library, and every copy
# carries the same identity -- install name @rpath/libSkyScriptLib.dylib on
# macOS, SONAME libSkyScriptLib.so on Linux, module name SkyScriptLib.dll on
# Windows. Dynamic loaders key loaded images by that identity, not by path. So
# the second SkyScript plugin X-Plane loads does not get its own copy; it gets
# the first one's, and inherits its globals: the Path singleton holding the
# plugin directory, the app registry, the dataref bindings. The symptom is a
# plugin that starts cleanly, reports loading an app, and then shows another
# plugin's window -- while never reading its own manifest.
#
# Run this over a packaged plugin folder as the last step before zipping it.
#
# Usage:
#   unique-library-identity.sh <plugin-dir> <plugin-name>
#
# <plugin-dir> is the folder that goes into Resources/plugins -- the one holding
# mac_x64/, win_x64/ and lin_x64/. Missing platforms are skipped, so packaging
# on one machine is fine.

if [ "$#" -ne 2 ]; then
  echo "usage: unique-library-identity.sh <plugin-dir> <plugin-name>" >&2
  exit 2
fi

plugin_dir="$1"
plugin_name="$2"

if [ ! -d "${plugin_dir}" ]; then
  echo "Not a directory: ${plugin_dir}" >&2
  exit 1
fi

# Anything that ends up in a filename and a Mach-O load command should not carry
# surprises. Keep it to what a plugin folder name would already contain.
if ! printf '%s' "${plugin_name}" | grep -qE '^[A-Za-z0-9._-]+$'; then
  echo "Plugin name must be alphanumeric, dot, dash or underscore: ${plugin_name}" >&2
  exit 1
fi

changed=0

# ------------------------------------------------------------------- macOS

mac_library="${plugin_dir}/mac_x64/libSkyScriptLib.dylib"
if [ -f "${mac_library}" ]; then
  if ! command -v install_name_tool >/dev/null 2>&1; then
    echo "install_name_tool not found; macOS libraries can only be renamed on macOS." >&2
    exit 1
  fi

  unique="libSkyScriptLib-${plugin_name}.dylib"
  mv "${mac_library}" "${plugin_dir}/mac_x64/${unique}"
  install_name_tool -id "@rpath/${unique}" "${plugin_dir}/mac_x64/${unique}"

  for xpl in "${plugin_dir}"/mac_x64/*.xpl; do
    [ -f "${xpl}" ] || continue
    install_name_tool -change "@rpath/libSkyScriptLib.dylib" "@rpath/${unique}" "${xpl}"

    # Editing a Mach-O invalidates its signature, and arm64 refuses to load a
    # modified binary whose signature no longer matches. Ad-hoc signing is what
    # makes the rename survive being loaded at all.
    codesign --force --sign - "${xpl}"
  done

  codesign --force --sign - "${plugin_dir}/mac_x64/${unique}"
  echo "mac_x64: ${unique}"
  changed=$((changed + 1))
fi

# ------------------------------------------------------------------- Linux

lin_library="${plugin_dir}/lin_x64/libSkyScriptLib.so"
if [ -f "${lin_library}" ]; then
  if ! command -v patchelf >/dev/null 2>&1; then
    echo "patchelf not found; install it to rename the Linux library." >&2
    echo "  apt-get install patchelf  /  brew install patchelf" >&2
    exit 1
  fi

  unique="libSkyScriptLib-${plugin_name}.so"
  mv "${lin_library}" "${plugin_dir}/lin_x64/${unique}"
  patchelf --set-soname "${unique}" "${plugin_dir}/lin_x64/${unique}"

  for xpl in "${plugin_dir}"/lin_x64/*.xpl; do
    [ -f "${xpl}" ] || continue
    patchelf --replace-needed "libSkyScriptLib.so" "${unique}" "${xpl}"
  done

  echo "lin_x64: ${unique}"
  changed=$((changed + 1))
fi

# ----------------------------------------------------------------- Windows

# Windows keys DLLs by module base name, so two SkyScriptLib.dll in different
# folders collide exactly as the other two platforms do. There is no
# install_name_tool for PE, and the import library baked into the .xpl records
# the DLL name as a plain string in the import descriptor -- so the rename is
# done by patching that string in place.
#
# In place means the new name must be exactly as long as the old one.
# "SkyScriptLib.dll" is 16 characters, and "SkyS" + 8 hex + ".dll" is also 16,
# so the unique name is derived from a hash of the plugin name rather than from
# the name itself. It is not pretty, and it does not need to be: nothing reads
# it but the loader.
win_library="${plugin_dir}/win_x64/SkyScriptLib.dll"
if [ -f "${win_library}" ]; then
  old_name="SkyScriptLib.dll"
  digest="$(printf '%s' "${plugin_name}" | shasum -a 256 | cut -c1-8)"
  unique="SkyS${digest}.dll"

  if [ "${#unique}" -ne "${#old_name}" ]; then
    echo "Internal error: '${unique}' is not ${#old_name} characters." >&2
    exit 1
  fi

  patched_any=0
  for xpl in "${plugin_dir}"/win_x64/*.xpl; do
    [ -f "${xpl}" ] || continue

    if ! grep -q "${old_name}" "${xpl}"; then
      echo "warning: ${xpl} does not import ${old_name}; leaving it alone" >&2
      continue
    fi

    # LC_ALL=C so sed works over bytes rather than trying to decode the binary
    # as text, which would mangle everything that is not the name.
    LC_ALL=C sed -i.bak "s/${old_name}/${unique}/g" "${xpl}"
    rm -f "${xpl}.bak"
    patched_any=1
  done

  if [ "${patched_any}" -eq 1 ]; then
    mv "${win_library}" "${plugin_dir}/win_x64/${unique}"
    echo "win_x64: ${unique}"
    changed=$((changed + 1))
  else
    echo "win_x64: no .xpl imported ${old_name}; DLL left as is" >&2
  fi
fi

if [ "${changed}" -eq 0 ]; then
  echo "No SkyScript libraries found under ${plugin_dir}." >&2
  exit 1
fi

echo "Gave ${changed} platform(s) an identity unique to '${plugin_name}'."
