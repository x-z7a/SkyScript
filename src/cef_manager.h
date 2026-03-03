#pragma once

#include <string>

/**
 * @brief Singleton that manages SkyScript's use of X-Plane's CEF instance.
 *
 * X-Plane initializes CEF for its own use before any plugin is loaded.
 * There can only be one CEF instance per process, so this plugin operates
 * in "client mode": it creates CefBrowser instances (like tabs) inside
 * X-Plane's already-running CEF context without ever calling
 * CefInitialize() or CefShutdown().
 *
 *   - Initialize() marks the manager as ready for browser creation.
 *   - Shutdown()   releases our readiness flag; does NOT call CefShutdown().
 *   - X-Plane drives CEF's message loop internally; DoMessageLoopWork()
 *     is therefore a no-op.
 *
 * When built without SKYSCRIPT_CEF_ENABLED, all methods are no-ops and
 * IsInitialized() always returns false.
 */
class CefManager
{
public:
    static CefManager &instance();

    /**
     * Mark the CEF client as ready.  Must be called from X-Plane's main
     * thread before any CefApp instance is created.
     *
     * Does NOT call CefInitialize() — X-Plane owns the CEF process lifetime.
     *
     * @param plugin_dir  Unused (kept for call-site compatibility).
     * @param cache_dir   Unused (X-Plane controls the CEF cache).
     */
    void Initialize(const std::string &plugin_dir, const std::string &cache_dir);

    /**
     * Mark the CEF client as no longer ready.  Called from Manager::stop().
     *
     * Does NOT call CefShutdown() — X-Plane owns the CEF process lifetime.
     */
    void Shutdown();

    /**
     * No-op.  X-Plane drives CEF's message loop internally; plugins must
     * not call CefDoMessageLoopWork().
     */
    void DoMessageLoopWork();

    /** Returns true once Initialize() has succeeded. */
    bool IsInitialized() const { return initialized_; }

private:
    CefManager()  = default;
    ~CefManager() = default;
    CefManager(const CefManager &) = delete;
    CefManager &operator=(const CefManager &) = delete;

    bool initialized_ = false;
};
