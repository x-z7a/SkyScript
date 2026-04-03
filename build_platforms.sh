#!/bin/sh

set -eu

AVAILABLE_PLATFORMS="mac win lin"
DEFAULT_XPLANE_SDK_ROOT="/Volumes/storage/git/SDK"
MAC_PLUGIN_COPY_TARGET="/Volumes/storage/X-Plane 12/Resources/plugins/SkyScript/mac.xpl"
PROJECT_NAME=$(sed -n 's/^#define PRODUCT_NAME "\(.*\)"/\1/p' src/include/config.h | head -n 1)
VERSION=$(sed -n 's/^#define VERSION "\(.*\)"/\1/p' src/include/config.h | head -n 1)
XPLANE_SDK_ROOT="${XPLANE_SDK_ROOT:-$DEFAULT_XPLANE_SDK_ROOT}"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
PLATFORMS=""
XPLANE_VERSION=12
CLEAN_BUILD=n
EXTRA_FILES=n

usage() {
    cat <<EOF
Usage: ./build_platforms.sh [options] [platform...]

Build SkyScript for one or more target platforms and create distribution bundles.

Platforms:
  mac
  win
  lin

Options:
  -p, --platforms LIST       Space- or comma-separated platforms to build
  -x, --xplane-version VER   X-Plane version to target (default: 12)
  -s, --sdk-root PATH        X-Plane SDK root (default: $XPLANE_SDK_ROOT)
  -j, --jobs COUNT           Parallel build jobs for non-Linux builds
  -c, --clean                Remove the build directory before building
  -e, --extra-files          Package additional plugin files when available
      --list-platforms       Print the supported platform names and exit
  -h, --help                 Show this help message and exit

Examples:
  ./build_platforms.sh mac
  ./build_platforms.sh --platforms mac,win --clean
  ./build_platforms.sh -p "mac lin" --extra-files
  ./build_platforms.sh --sdk-root /path/to/SDK mac
EOF
}

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

is_valid_platform() {
    case "$1" in
        mac|win|lin) return 0 ;;
        *) return 1 ;;
    esac
}

has_platform() {
    candidate=$1

    for existing in $PLATFORMS; do
        if [ "$existing" = "$candidate" ]; then
            return 0
        fi
    done

    return 1
}

add_platforms() {
    raw_platforms=$1
    non_separator_chars=$(printf '%s' "$raw_platforms" | tr -d '[:space:],')

    [ -n "$non_separator_chars" ] || die "Platform list cannot be empty."

    for platform in $(printf '%s' "$raw_platforms" | tr ',' ' '); do
        is_valid_platform "$platform" || die "Invalid platform: $platform. Supported platforms: $AVAILABLE_PLATFORMS"

        if ! has_platform "$platform"; then
            PLATFORMS="${PLATFORMS}${PLATFORMS:+ }$platform"
        fi
    done
}

ensure_positive_integer() {
    value=$1
    label=$2

    case "$value" in
        ''|*[!0-9]*)
            die "$label must be a positive integer."
            ;;
    esac

    [ "$value" -ge 1 ] || die "$label must be a positive integer."
}

copy_artifact() {
    source_path=$1
    destination_path=$2

    if [ ! -f "$source_path" ]; then
        printf 'Skipping copy; source not found: %s\n' "$source_path"
        return 0
    fi

    mkdir -p "$(dirname "$destination_path")"
    cp "$source_path" "$destination_path"
    printf 'Copied %s -> %s\n' "$source_path" "$destination_path"
}

run_additional_copies() {
    if has_platform mac; then
        copy_artifact "build/dist/$PROJECT_NAME-example/mac_x64/$PROJECT_NAME.xpl" "$MAC_PLUGIN_COPY_TARGET"

        plugin_dir=$(dirname "$MAC_PLUGIN_COPY_TARGET")
        if [ -d "build/dist/$PROJECT_NAME-example/apps" ]; then
            rm -rf "$plugin_dir/apps"
            cp -r "build/dist/$PROJECT_NAME-example/apps" "$plugin_dir/apps"
            printf 'Copied apps -> %s/apps\n' "$plugin_dir"
        fi
    fi
}

[ -n "$PROJECT_NAME" ] || die "Unable to read PRODUCT_NAME from src/include/config.h"
[ -n "$VERSION" ] || die "Unable to read VERSION from src/include/config.h"

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --list-platforms)
            for platform in $AVAILABLE_PLATFORMS; do
                printf '%s\n' "$platform"
            done
            exit 0
            ;;
        -p|--platforms)
            [ "$#" -ge 2 ] || die "Missing value for $1"
            add_platforms "$2"
            shift 2
            ;;
        --platforms=*)
            add_platforms "${1#*=}"
            shift
            ;;
        -x|--xplane-version)
            [ "$#" -ge 2 ] || die "Missing value for $1"
            XPLANE_VERSION=$2
            shift 2
            ;;
        --xplane-version=*)
            XPLANE_VERSION=${1#*=}
            shift
            ;;
        -s|--sdk-root)
            [ "$#" -ge 2 ] || die "Missing value for $1"
            XPLANE_SDK_ROOT=$2
            shift 2
            ;;
        --sdk-root=*)
            XPLANE_SDK_ROOT=${1#*=}
            shift
            ;;
        -j|--jobs)
            [ "$#" -ge 2 ] || die "Missing value for $1"
            JOBS=$2
            shift 2
            ;;
        --jobs=*)
            JOBS=${1#*=}
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=y
            shift
            ;;
        -e|--extra-files)
            EXTRA_FILES=y
            shift
            ;;
        --)
            shift
            while [ "$#" -gt 0 ]; do
                add_platforms "$1"
                shift
            done
            break
            ;;
        -*)
            die "Unknown option: $1"
            ;;
        *)
            add_platforms "$1"
            shift
            ;;
    esac
done

[ -n "$PLATFORMS" ] || PLATFORMS=$AVAILABLE_PLATFORMS
ensure_positive_integer "$JOBS" "Job count"

if [ "$XPLANE_VERSION" != "12" ]; then
    die "Only X-Plane 12 builds are supported."
fi

printf 'Building %s.xpl version %s\n' "$PROJECT_NAME" "$VERSION"
printf 'Platforms: %s\n' "$PLATFORMS"
printf 'X-Plane version: %s\n' "$XPLANE_VERSION"
printf 'SDK root: %s\n' "$XPLANE_SDK_ROOT"
printf 'Parallel jobs: %s\n' "$JOBS"
printf 'Clean build: %s\n' "$CLEAN_BUILD"
printf 'Package extra files: %s\n\n' "$EXTRA_FILES"

if [ "$CLEAN_BUILD" = "y" ] && [ -d "build" ]; then
    printf 'Cleaning build directory...\n'
    rm -rf build
fi

if has_platform mac; then
    [ -d "$XPLANE_SDK_ROOT/CHeaders/XPLM" ] || die "Missing X-Plane SDK headers at $XPLANE_SDK_ROOT"
    [ -d "$XPLANE_SDK_ROOT/CHeaders/Widgets" ] || die "Missing X-Plane Widgets headers at $XPLANE_SDK_ROOT"
    [ -d "$XPLANE_SDK_ROOT/CHeaders/Wrappers" ] || die "Missing X-Plane Wrappers headers at $XPLANE_SDK_ROOT"
    [ -f "$XPLANE_SDK_ROOT/Libraries/Mac/XPLM.framework/XPLM" ] || die "Missing XPLM.framework at $XPLANE_SDK_ROOT"
    [ -f "$XPLANE_SDK_ROOT/Libraries/Mac/XPWidgets.framework/XPWidgets" ] || die "Missing XPWidgets.framework at $XPLANE_SDK_ROOT"
    [ -d "lib/mac_x64/cef/include" ] || die "Missing CEF headers under lib/mac_x64/cef"
    [ -f "lib/mac_x64/cef/Release/Chromium Embedded Framework.framework/Chromium Embedded Framework" ] || die "Missing macOS CEF framework under lib/mac_x64/cef/Release"
    [ -f "lib/mac_x64/cef/libcef_dll_wrapper.a" ] || die "Missing lib/mac_x64/cef/libcef_dll_wrapper.a"
fi

for platform in $PLATFORMS; do
    printf 'Building %s...\n' "$platform"

    if [ "$platform" = "lin" ]; then
        docker build -t gcc-cmake -f ./docker/Dockerfile.linux . &&
        docker run --user "$(id -u):$(id -g)" --rm -e XPLANE_SDK_ROOT="$XPLANE_SDK_ROOT" -v "$(pwd):/src" -w /src gcc-cmake:latest bash -c "\
        cmake -DCMAKE_CXX_FLAGS='-march=x86-64' -DCMAKE_TOOLCHAIN_FILE=toolchain-$platform.cmake -DXPLANE_VERSION=$XPLANE_VERSION -DXPLANE_SDK_ROOT=\"\$XPLANE_SDK_ROOT\" -Bbuild/$platform -H. && \
        cmake --build build/$platform --parallel \$(nproc)"
    else
        cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-"$platform".cmake -DCMAKE_OSX_ARCHITECTURES=arm64 -DXPLANE_VERSION="$XPLANE_VERSION" -DXPLANE_SDK_ROOT="$XPLANE_SDK_ROOT" -Bbuild/"$platform" -H.
        cmake --build build/"$platform" --parallel "$JOBS"
    fi

    printf '\n%s build succeeded.\n' "$platform"
    printf 'Product: build/%s/%s_x64/%s.xpl\n' "$platform" "$platform" "$PROJECT_NAME"
    file build/"$platform"/"${platform}"_x64/"$PROJECT_NAME".xpl
    printf '\n'
    sleep 1
done

printf 'Building has finished.\n'

# ---- Build apps (React/Node) ----
if [ -d "apps" ]; then
    for app_dir in apps/*/; do
        if [ -f "${app_dir}package.json" ]; then
            app_name=$(basename "$app_dir")
            printf 'Building app: %s\n' "$app_name"
            (cd "$app_dir" && npm install --silent && npm run build --silent)
            printf 'App %s built successfully.\n' "$app_name"
        fi
    done
fi

printf 'Creating distribution bundles...\n'

if [ -d "build/dist" ]; then
    rm -rf build/dist
fi

# ---- Library distribution (static lib + headers) ----
LIB_DIST="build/dist/$PROJECT_NAME-lib"
mkdir -p "$LIB_DIST/include"

# Copy public headers (skyscript.h, C API header + transitive dependencies)
cp src/skyscript.h "$LIB_DIST/include/"
cp src/skyscript_c.h "$LIB_DIST/include/"
cp src/include/app.h "$LIB_DIST/include/"
cp src/include/config.h "$LIB_DIST/include/"
cp src/include/xplm_bridge.h "$LIB_DIST/include/"
mkdir -p "$LIB_DIST/include/components"
cp src/include/components/button.h "$LIB_DIST/include/components/"
cp src/include/components/image.h "$LIB_DIST/include/components/"
cp src/include/components/notification.h "$LIB_DIST/include/components/"
mkdir -p "$LIB_DIST/include/utils"
cp src/include/utils/dataref.h "$LIB_DIST/include/utils/"
cp src/include/utils/path.h "$LIB_DIST/include/utils/"
mkdir -p "$LIB_DIST/include/utils/cursor"
if [ -d "src/include/utils/cursor" ]; then
    cp src/include/utils/cursor/*.h "$LIB_DIST/include/utils/cursor/" 2>/dev/null || true
fi

# Copy static library for each built platform
for platform in $PLATFORMS; do
    if [ "$platform" = "win" ]; then
        lib_file="build/$platform/SkyScriptLib.lib"
        if [ ! -f "$lib_file" ]; then
            lib_file="build/$platform/libSkyScriptLib.a"
        fi
    else
        lib_file="build/$platform/libSkyScriptLib.a"
    fi

    if [ -f "$lib_file" ]; then
        mkdir -p "$LIB_DIST/lib/${platform}_x64"
        cp "$lib_file" "$LIB_DIST/lib/${platform}_x64/"
        printf 'Bundled library: %s -> %s\n' "$lib_file" "${platform}_x64"
    else
        printf 'Warning: static library not found for %s at %s\n' "$platform" "$lib_file"
    fi
done

# Copy Go bindings
cp go.mod "$LIB_DIST/"
cp -r go "$LIB_DIST/"

# ---- Example plugin distribution (.xpl + apps + assets + source + libs) ----
EXAMPLE_DIST="build/dist/$PROJECT_NAME-example"

# Include example source code and apps
mkdir -p "$EXAMPLE_DIST/example"
cp example/main.cpp "$EXAMPLE_DIST/example/"
cp -r apps "$EXAMPLE_DIST/example/"

# Include library headers and static libs so developers can build from source
cp -r "$LIB_DIST/include" "$EXAMPLE_DIST/"
if [ -d "$LIB_DIST/lib" ]; then
    cp -r "$LIB_DIST/lib" "$EXAMPLE_DIST/"
fi

for platform in $AVAILABLE_PLATFORMS; do
    mkdir -p "$EXAMPLE_DIST/${platform}_x64"

    if [ -d "build/$platform/${platform}_x64" ]; then
        cp build/"$platform"/"${platform}"_x64/"$PROJECT_NAME".xpl "$EXAMPLE_DIST"/"${platform}"_x64/"$PROJECT_NAME".xpl
    fi

    if has_platform "$platform" && [ -d "lib/${platform}_x64/dist_${XPLANE_VERSION}" ]; then
        cp -r lib/"${platform}"_x64/"dist_${XPLANE_VERSION}"/* "$EXAMPLE_DIST"/"${platform}"_x64
    fi

    if has_platform "$platform" && [ -d "lib/${platform}_x64/dist_extra_${XPLANE_VERSION}" ] && [ "$EXTRA_FILES" = "y" ]; then
        mkdir -p build/"extra_${platform}"/"${platform}"_x64
        cp -r lib/"${platform}"_x64/"dist_extra_${XPLANE_VERSION}"/* build/"extra_${platform}"/"${platform}"_x64
    fi
done

cp -r assets "$EXAMPLE_DIST"

# Bundle built apps into example
if [ -d "apps" ]; then
    mkdir -p "$EXAMPLE_DIST/apps"
    for app_dir in apps/*/; do
        app_name=$(basename "$app_dir")
        if [ -d "${app_dir}build" ]; then
            cp -r "${app_dir}build" "$EXAMPLE_DIST/apps/${app_name}"
            if [ -f "${app_dir}manifest.yaml" ]; then
                cp "${app_dir}manifest.yaml" "$EXAMPLE_DIST/apps/${app_name}/"
            fi
            printf 'Bundled app: %s\n' "$app_name"
        elif [ -f "${app_dir}manifest.yaml" ]; then
            mkdir -p "$EXAMPLE_DIST/apps/${app_name}"
            cp "${app_dir}manifest.yaml" "$EXAMPLE_DIST/apps/${app_name}/"
            printf 'Bundled app (manifest only): %s\n' "$app_name"
        fi
    done
fi

if [ "$XPLANE_VERSION" -ge 12 ]; then
    cat > "$EXAMPLE_DIST/skunkcrafts_updater.cfg" <<EOF
module|https://github.com/x-z7a/skyscript
name|SkyScript
version|$VERSION
locked|false
disabled|false
zone|custom
EOF
fi

# ---- Create zip archives ----
cd build/dist

VERSION_SUFFIX=$VERSION-XP$XPLANE_VERSION

# Library zip
rm -f "$PROJECT_NAME-lib-$VERSION_SUFFIX.zip"
zip -rq "$PROJECT_NAME-lib-$VERSION_SUFFIX.zip" "$PROJECT_NAME-lib" -x "*/.DS_Store" -x "*/__MACOSX/*"
printf 'Library bundle: build/dist/%s-lib-%s.zip\n' "$PROJECT_NAME" "$VERSION_SUFFIX"

# Example zip
rm -f "$PROJECT_NAME-example-$VERSION_SUFFIX.zip"
zip -rq "$PROJECT_NAME-example-$VERSION_SUFFIX.zip" "$PROJECT_NAME-example" -x "*/.DS_Store" -x "*/__MACOSX/*"
printf 'Example bundle: build/dist/%s-example-%s.zip\n' "$PROJECT_NAME" "$VERSION_SUFFIX"

if [ "$EXTRA_FILES" = "y" ]; then
    cd ..
    for platform in $PLATFORMS; do
        if [ -d "extra_${platform}" ]; then
            cat > "extra_${platform}/README.txt" <<EOF
The '${platform}_x64' folder contains additional files required by the $PROJECT_NAME plugin for XP$XPLANE_VERSION.
Unzip and merge these files with the plugin folder in your X-Plane installation directory, usually located at 'Resources/plugins/$PROJECT_NAME/${platform}_x64'.
EOF
            rm -f "XP$XPLANE_VERSION-$platform-additional-files.zip"
            zip -rq "XP$XPLANE_VERSION-$platform-additional-files.zip" "extra_${platform}" -x ".DS_Store" -x "__MACOSX"
        fi
    done
    cd dist
fi

cd ../..

run_additional_copies

printf 'Distribution complete.\n'
