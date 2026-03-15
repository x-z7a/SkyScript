#!/bin/sh

set -eu

AVAILABLE_PLATFORMS="mac win lin"
DEFAULT_XPLANE_SDK_ROOT="/Volumes/storage/git/SkyScript/SDK"
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
        copy_artifact "build/dist/mac_x64/$PROJECT_NAME.xpl" "$MAC_PLUGIN_COPY_TARGET"
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
printf 'Creating distribution bundle...\n'

if [ -d "build/dist" ]; then
    rm -rf build/dist
fi

for platform in $AVAILABLE_PLATFORMS; do
    mkdir -p build/dist/"${platform}"_x64

    if [ -d "build/$platform/${platform}_x64" ]; then
        cp build/"$platform"/"${platform}"_x64/"$PROJECT_NAME".xpl build/dist/"${platform}"_x64/"$PROJECT_NAME".xpl
    fi

    if has_platform "$platform" && [ -d "lib/${platform}_x64/dist_${XPLANE_VERSION}" ]; then
        cp -r lib/"${platform}"_x64/"dist_${XPLANE_VERSION}"/* build/dist/"${platform}"_x64
    fi

    if has_platform "$platform" && [ -d "lib/${platform}_x64/dist_extra_${XPLANE_VERSION}" ] && [ "$EXTRA_FILES" = "y" ]; then
        mkdir -p build/"extra_${platform}"/"${platform}"_x64
        cp -r lib/"${platform}"_x64/"dist_extra_${XPLANE_VERSION}"/* build/"extra_${platform}"/"${platform}"_x64
    fi
done

cp -r assets build/dist

if [ "$XPLANE_VERSION" -ge 12 ]; then
    cat > build/dist/skunkcrafts_updater.cfg <<EOF
module|https://github.com/x-z7a/skyscript-cef
name|SkyScript
version|$VERSION
locked|false
disabled|false
zone|custom
EOF
fi

cd build
mv dist "$PROJECT_NAME"

VERSION=$VERSION-XP$XPLANE_VERSION

rm -f "$PROJECT_NAME-$VERSION.zip"
zip -rq "$PROJECT_NAME-$VERSION.zip" "$PROJECT_NAME" -x "*/.DS_Store" -x "*/__MACOSX/*"

if [ "$EXTRA_FILES" = "y" ]; then
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
fi

mv "$PROJECT_NAME" dist
mv "$PROJECT_NAME-$VERSION.zip" dist/
cd ..

run_additional_copies

printf 'Bundle created. Distribution: build/dist/%s-%s.zip\n' "$PROJECT_NAME" "$VERSION"
