/**
 * cef_subprocess.cpp — Entry point for the SkyScript CEF helper subprocess.
 *
 * This file is compiled into a SEPARATE EXECUTABLE (SkyScript_CEF_Helper on
 * macOS, skyscript-cef-helper on Linux, skyscript-cef-helper.exe on Windows).
 * It is NOT linked into the plugin (.xpl).
 *
 * CEF spawns this process for renderer, GPU, and utility sub-processes.
 * The only thing it needs to do is call CefExecuteProcess() and exit.
 *
 * Build notes:
 *   macOS : Wrapped in SkyScript_CEF_Helper.app bundle with Info.plist.
 *           Requires ad-hoc code-signing for Gatekeeper:
 *             codesign --force --sign - SkyScript_CEF_Helper.app
 *   Linux : Plain ELF binary, no special treatment required.
 *   Windows: Plain PE binary (skyscript-cef-helper.exe).
 *
 * See Makefile.mac64 / Makefile.lin64 / build-win-msvc.bat for the
 * separate helper target.
 */

#ifdef SKYSCRIPT_CEF_ENABLED

#include "include/cef_app.h"

#if defined(_WIN32)
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    CefMainArgs main_args(hInstance);
    return CefExecuteProcess(main_args, nullptr, nullptr);
}

#else // macOS / Linux

int main(int argc, char *argv[])
{
    CefMainArgs main_args(argc, argv);
    return CefExecuteProcess(main_args, nullptr, nullptr);
}

#endif // _WIN32

#else // SKYSCRIPT_CEF_ENABLED not defined

// Stub main so this file still compiles when CEF is not available.
#if defined(_WIN32)
#include <windows.h>
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPTSTR, int) { return 0; }
#else
int main(int, char **) { return 0; }
#endif

#endif // SKYSCRIPT_CEF_ENABLED
