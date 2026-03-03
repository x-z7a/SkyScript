@echo off
:: fetch-cef.bat — Download and prepare a CEF binary distribution for SkyScript (Windows).
::
:: Run once before building with CEF support:
::   scripts\fetch-cef.bat
::
:: Requires: curl (built-in on Windows 10+), cmake, 7-Zip or tar (Windows 10+).

setlocal enabledelayedexpansion

:: ── Configuration ─────────────────────────────────────────────────────────────
if "%CEF_VERSION%"=="" set CEF_VERSION=132.0.3+g9e7f85e+chromium-132.0.6834.111

set CDN_BASE=https://cef-builds.spotifycdn.com
set PLATFORM=windows64
set TARBALL=cef_binary_%CEF_VERSION%_%PLATFORM%.tar.bz2
set URL=%CDN_BASE%/%TARBALL%

:: URL-encode '+' for the query (curl handles most encoding, but '+' in version needs escaping)
set ENCODED_URL=%URL:+=%%2B%

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..
set DEST=%REPO_ROOT%\cef_binary_%CEF_VERSION%_%PLATFORM%

echo [fetch-cef] CEF version: %CEF_VERSION%
echo [fetch-cef] Repository: %REPO_ROOT%

if exist "%DEST%" (
    echo [fetch-cef] Already exists: %DEST%  (skipping download^)
    goto build
)

echo [fetch-cef] Downloading: %ENCODED_URL%
curl -L --progress-bar -o "%TEMP%\%TARBALL%" "%ENCODED_URL%"
if errorlevel 1 (
    echo [fetch-cef] ERROR: Download failed.
    exit /b 1
)

echo [fetch-cef] Extracting...
:: Windows 10+ tar supports bz2
tar -xjf "%TEMP%\%TARBALL%" -C "%REPO_ROOT%"
del "%TEMP%\%TARBALL%"

:build
echo [fetch-cef] Building libcef_dll_wrapper...
if exist "%DEST%\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" (
    echo [fetch-cef] libcef_dll_wrapper already built  (skipping^)
    goto done
)

mkdir "%DEST%\build" 2>nul
pushd "%DEST%\build"
cmake .. -DCEF_RUNTIME_LIBRARY_FLAG=/MT -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target libcef_dll_wrapper
popd

:done
echo.
echo [fetch-cef] Done. To build with CEF support, open the MSVC solution in %DEST%\build
echo and add SKYSCRIPT_CEF_ENABLED to preprocessor definitions.

endlocal
