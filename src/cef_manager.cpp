#include "cef_manager.h"
#include "log_msg.h"

#ifdef SKYSCRIPT_CEF_ENABLED
// ── CEF headers (only available when CEF SDK is present) ─────────────────────
#include "include/cef_app.h"
#include "include/cef_base.h"
#endif

CefManager &CefManager::instance()
{
    static CefManager inst;
    return inst;
}

// ── Initialize ────────────────────────────────────────────────────────────────

void CefManager::Initialize(const std::string &plugin_dir, const std::string &cache_dir)
{
#ifdef SKYSCRIPT_CEF_ENABLED
    if (initialized_)
    {
        LogMsg("CefManager: already initialized — skipping (CEF is one-shot per process)");
        return;
    }

    LogMsg("CefManager: initializing CEF (plugin_dir=%s, cache_dir=%s)",
           plugin_dir.c_str(), cache_dir.c_str());

    CefMainArgs main_args;  // empty on macOS/Linux; populated by CefExecuteProcess in subprocess

    CefSettings settings;
    settings.no_sandbox                    = true;
    settings.windowless_rendering_enabled  = true;
    settings.multi_threaded_message_loop   = false;
    settings.command_line_args_disabled    = false;

    CefString(&settings.cache_path).FromASCII(cache_dir.c_str());

    // Helper subprocess binary — must exist in the plugin package.
#if APL
    std::string helper_path = plugin_dir + "/SkyScript_CEF_Helper.app/Contents/MacOS/SkyScript_CEF_Helper";
#elif LIN
    std::string helper_path = plugin_dir + "/skyscript-cef-helper";
#elif IBM
    std::string helper_path = plugin_dir + "/skyscript-cef-helper.exe";
#endif
    CefString(&settings.browser_subprocess_path).FromASCII(helper_path.c_str());

    bool ok = CefInitialize(main_args, settings, nullptr, nullptr);
    if (!ok)
    {
        LogMsg("CefManager: CefInitialize FAILED");
        return;
    }

    initialized_ = true;
    LogMsg("CefManager: CEF initialized successfully");
#else
    LogMsg("CefManager: CEF support not compiled in (SKYSCRIPT_CEF_ENABLED not defined)");
    (void)plugin_dir;
    (void)cache_dir;
#endif
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

void CefManager::Shutdown()
{
#ifdef SKYSCRIPT_CEF_ENABLED
    if (!initialized_)
        return;

    LogMsg("CefManager: shutting down CEF");
    CefShutdown();
    initialized_ = false;
    LogMsg("CefManager: CEF shutdown complete");
#endif
}

// ── Message loop ──────────────────────────────────────────────────────────────

void CefManager::DoMessageLoopWork()
{
#ifdef SKYSCRIPT_CEF_ENABLED
    if (!initialized_)
        return;

    // With multi_threaded_message_loop=false, we drive CEF's loop manually
    // from X-Plane's flight loop so that OnPaint callbacks arrive on the main
    // thread (same thread as the GL context).
    CefDoMessageLoopWork();
#endif
}
