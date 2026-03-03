#pragma once

#include <string>

/**
 * @brief Singleton that owns the CEF process lifetime.
 *
 * CEF CANNOT be re-initialized after CefShutdown() is called in the same
 * process.  Therefore:
 *   - CefInitialize is called once in Manager::initialize() → CefManager::Initialize()
 *   - CefShutdown  is called once in Manager::stop()       → CefManager::Shutdown()
 *   - XPluginDisable / XPluginEnable only destroy/recreate CefBrowserHost instances;
 *     they do NOT call Shutdown()/Initialize() again.
 *
 * When built without SKYSCRIPT_CEF_ENABLED, all methods are no-ops and
 * IsInitialized() always returns false.
 */
class CefManager
{
public:
    static CefManager &instance();

    /**
     * Initialize CEF.  Must be called from X-Plane's main thread, once,
     * before any CefApp instance is created.
     *
     * @param plugin_dir  Absolute path to the SkyScript plugin folder
     *                    (used to locate the helper subprocess binary).
     * @param cache_dir   Absolute path for CEF's disk cache.
     */
    void Initialize(const std::string &plugin_dir, const std::string &cache_dir);

    /**
     * Shut down CEF.  Called exactly once from Manager::stop() (XPluginStop).
     * Must not be called from XPluginDisable — only from XPluginStop.
     */
    void Shutdown();

    /**
     * Drive CEF's message loop.  Call every frame from the X-Plane flight
     * loop callback.  This is a no-op when multi_threaded_message_loop is
     * false (our configuration).
     *
     * Must NOT be called after Shutdown().
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
