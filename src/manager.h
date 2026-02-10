
#pragma once
#include <cstring>
#include <string>
#include <memory>
#include <filesystem>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "XPLMDataAccess.h"
#include "XPLMScenery.h"
#include "XPLMDisplay.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMMenus.h"

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/Platform.h>
#include <AppCore/JSHelpers.h>

#include "log_msg.h"
#include "../version.h"
#include "app.h"
using namespace ultralight;
class Manager
{

public:
    enum class UltralightTaskPriority : uint8_t
    {
        High,
        Normal,
    };

    static Manager &instance();

    RefPtr<Renderer> renderer_;

    int initialize(char *out_name, char *out_sig, char *out_desc);
    void enable();
    void disable();
    static void menuCB([[maybe_unused]] void *menu_ref, void *item_ref);
    void discoverApps();
    void initializeAllApps();
    void destroyAllApps();
    void updateAllApps();
    void drawAllApps();
    void forceRepaintAllApps();
    void stop();
    void postToUltralightThread(std::function<void()> task,
                                UltralightTaskPriority priority = UltralightTaskPriority::Normal);
    void requestUltralightUpdate();
    void requestUltralightRender();
    bool isUltralightThread() const;
    bool isRendererReady() const { return renderer_ready_.load(); }

    // Plugin info getters
    const char *getName() const { return name; }
    const char *getSignature() const { return signature; }
    const char *getDescription() const { return description; }

    // Accessors
    const std::string &getXpDir() const { return xp_dir; }
    const std::string &getPluginDir() const { return plugin_dir; }
    const std::string &getOutputDir() const { return output_dir; }
    const std::string &getPrefPath() const { return pref_path; }

    // Setters
    void setXpDir(const std::string &v) { xp_dir = v; }
    void setPluginDir(const std::string &v) { plugin_dir = v; }
    void setOutputDir(const std::string &v) { output_dir = v; }
    void setPrefPath(const std::string &v) { pref_path = v; }

    // App management API (for SkyScript JS bindings)
    std::vector<std::string> getAppNames() const;
    bool reloadApp(const std::string& name);
    bool openAppWindow(const std::string& name);
    bool openAppInspector(const std::string& name);

private:
    // ...existing code...
    char name[128] = {};
    char signature[128] = {};
    char description[256] = {};

    std::string app_name, xp_dir, plugin_dir, output_dir, pref_path;
    XPLMMenuID menu_;

    std::unordered_map<std::string, std::unique_ptr<App>> apps_;
    std::thread ultralight_thread_;
    std::thread::id ultralight_thread_id_;
    std::mutex ultralight_mutex_;
    std::condition_variable ultralight_cv_;
    std::deque<std::function<void()>> ultralight_high_priority_tasks_;
    std::deque<std::function<void()>> ultralight_normal_priority_tasks_;
    std::atomic<uint32_t> pending_updates_{0};
    std::atomic<uint32_t> pending_renders_{0};
    std::atomic<bool> renderer_ready_{false};
    bool ultralight_thread_started_ = false;
    bool ultralight_thread_exited_ = true;
    bool ultralight_thread_stop_ = false;

    void startUltralightThread();
    void stopUltralightThread();
    void ultralightThreadMain();

private:
    Manager();
    ~Manager();
    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;
};
