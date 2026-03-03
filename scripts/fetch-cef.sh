#!/usr/bin/env bash
# fetch-cef.sh — Download and prepare a CEF binary distribution for SkyScript.
#
# Run once before building with CEF support:
#   ./scripts/fetch-cef.sh
#
# CEF binary distributions are sourced from the Spotify CDN (cef-builds.spotifycdn.com).
# They are ~130 MB compressed and are NOT vendored in the repo.
#
# After running this script, the following will exist in the repo root:
#   cef_binary_<version>_macosx_arm64/    (macOS ARM64 slice)
#   cef_binary_<version>_macosx_x86_64/   (macOS x86_64 slice)
#   cef_binary_<version>_linux64/         (Linux)
#
# To build with CEF support, pass SKYSCRIPT_CEF_ENABLED=1 to make.

set -euo pipefail

# ── Configuration ────────────────────────────────────────────────────────────
# CEF version to download. Check https://cef-builds.spotifycdn.com/index.html
# for the latest stable build that supports Off-Screen Rendering (OSR).
CEF_VERSION="${CEF_VERSION:-132.0.3+g9e7f85e+chromium-132.0.6834.111}"
CEF_VERSION_ENCODED=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$CEF_VERSION', safe=''))")

CDN_BASE="https://cef-builds.spotifycdn.com"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Platform detection ────────────────────────────────────────────────────────
OS="$(uname -s)"
ARCH="$(uname -m)"

# ── Helpers ───────────────────────────────────────────────────────────────────
download_and_extract() {
    local platform="$1"
    local tarball="cef_binary_${CEF_VERSION}_${platform}.tar.bz2"
    local url="${CDN_BASE}/${tarball}"
    local dest="${REPO_ROOT}/cef_binary_${CEF_VERSION}_${platform}"

    if [ -d "$dest" ]; then
        echo "[fetch-cef] Already exists: $dest  (skipping download)"
        return 0
    fi

    echo "[fetch-cef] Downloading: $url"
    curl -L --progress-bar -o "/tmp/${tarball}" "$url"

    echo "[fetch-cef] Extracting to $dest ..."
    tar -xjf "/tmp/${tarball}" -C "$REPO_ROOT"
    rm -f "/tmp/${tarball}"
    echo "[fetch-cef] Extracted: $dest"
}

build_wrapper() {
    local platform="$1"
    local dest="${REPO_ROOT}/cef_binary_${CEF_VERSION}_${platform}"

    if [ ! -d "$dest" ]; then
        echo "[fetch-cef] ERROR: CEF directory not found: $dest"
        return 1
    fi

    local wrapper_dir="${dest}/libcef_dll_wrapper"
    if [ -f "${dest}/build/libcef_dll_wrapper/libcef_dll_wrapper.a" ]; then
        echo "[fetch-cef] libcef_dll_wrapper already built for $platform  (skipping)"
        return 0
    fi

    echo "[fetch-cef] Building libcef_dll_wrapper for $platform ..."
    mkdir -p "${dest}/build"
    cd "${dest}/build"

    local cmake_flags="-DCEF_RUNTIME_LIBRARY_FLAG=/MT"
    if [ "$platform" = "macosx_arm64" ]; then
        cmake_flags="$cmake_flags -DCMAKE_OSX_ARCHITECTURES=arm64"
    elif [ "$platform" = "macosx_x86_64" ]; then
        cmake_flags="$cmake_flags -DCMAKE_OSX_ARCHITECTURES=x86_64"
    fi

    cmake .. $cmake_flags -DCMAKE_BUILD_TYPE=Release
    make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" libcef_dll_wrapper

    echo "[fetch-cef] libcef_dll_wrapper built: ${dest}/build/libcef_dll_wrapper/"
    cd "$REPO_ROOT"
}

# ── Main ──────────────────────────────────────────────────────────────────────
echo "[fetch-cef] CEF version: $CEF_VERSION"
echo "[fetch-cef] Repository: $REPO_ROOT"

if [ "$OS" = "Darwin" ]; then
    echo "[fetch-cef] macOS detected — downloading both ARM64 and x86_64 slices"

    download_and_extract "macosx_arm64"
    download_and_extract "macosx_x86_64"

    build_wrapper "macosx_arm64"
    build_wrapper "macosx_x86_64"

    echo ""
    echo "[fetch-cef] Done. To build with CEF support:"
    echo "  make -f Makefile.mac64 SKYSCRIPT_CEF_ENABLED=1"

elif [ "$OS" = "Linux" ]; then
    echo "[fetch-cef] Linux detected — downloading linux64"

    download_and_extract "linux64"
    build_wrapper "linux64"

    echo ""
    echo "[fetch-cef] Done. To build with CEF support:"
    echo "  make -f Makefile.lin64 SKYSCRIPT_CEF_ENABLED=1"

else
    echo "[fetch-cef] Unsupported OS: $OS"
    echo "On Windows, run scripts/fetch-cef.bat instead."
    exit 1
fi
