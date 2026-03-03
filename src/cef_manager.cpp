#include "cef_manager.h"
#include "log_msg.h"

CefManager &CefManager::instance()
{
    static CefManager inst;
    return inst;
}

// ── Initialize ────────────────────────────────────────────────────────────────

void CefManager::Initialize(const std::string &plugin_dir, const std::string &cache_dir)
{
    (void)plugin_dir;
    (void)cache_dir;
#ifdef SKYSCRIPT_CEF_ENABLED
    if (initialized_)
    {
        LogMsg("CefManager: already ready — skipping");
        return;
    }

    // X-Plane owns the CEF process lifetime and has already called
    // CefInitialize() before any plugin is loaded.  We operate in
    // "client mode": we create CefBrowser instances (tabs) inside
    // X-Plane's existing CEF context without calling CefInitialize().
    initialized_ = true;
    LogMsg("CefManager: ready (attached to X-Plane's CEF instance)");
#else
    LogMsg("CefManager: CEF support not compiled in (SKYSCRIPT_CEF_ENABLED not defined)");
#endif
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

void CefManager::Shutdown()
{
#ifdef SKYSCRIPT_CEF_ENABLED
    if (!initialized_)
        return;

    // Do NOT call CefShutdown() — X-Plane owns the CEF process lifetime.
    // Browser instances are already destroyed by CefApp::Destroy() before
    // this point.
    initialized_ = false;
    LogMsg("CefManager: released (X-Plane retains the CEF instance)");
#endif
}

// ── Message loop ──────────────────────────────────────────────────────────────

void CefManager::DoMessageLoopWork()
{
    // X-Plane drives CEF's message loop internally.
    // Plugins must not call CefDoMessageLoopWork().
}
