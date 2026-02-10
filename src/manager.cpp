#include "manager.h"
#include "bindings/hid.h"
#include "bindings/utilities.h"
#include "main_thread_dispatcher.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <thread>
#include <utility>

namespace {

uint32_t ResolveUltralightRendererThreads()
{
    // Optional override for tuning: 0 lets Ultralight choose automatically.
    if (const char *env = std::getenv("SKYSCRIPT_UL_NUM_RENDERER_THREADS"))
    {
        const unsigned long parsed = std::strtoul(env, nullptr, 10);
        return static_cast<uint32_t>(std::min<unsigned long>(parsed, std::numeric_limits<uint32_t>::max()));
    }

    // Keep one core free for the sim/main thread.
    const unsigned int cores = std::thread::hardware_concurrency();
    if (cores <= 1)
    {
        return 1;
    }
    return static_cast<uint32_t>(cores - 1);
}

} // namespace

Manager &Manager::instance()
{
    static Manager instance;
    return instance;
}

Manager::Manager()
{
    app_name = "SkyScript";
    strcpy(name, (app_name + " - " VERSION_SHORT " - ").c_str());
    strcpy(signature, "com.github.x-z7a.skyscript");
    strcpy(description, "Powerfull JavaScript runtime for X-Plane plugins");

    // renderer_ = Renderer::Create();
}

Manager::~Manager()
{
    // Destructor code here
}

float update(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon)
{
    MainThreadDispatcher::instance().Drain(16, 128);

    if (Manager::instance().isRendererReady())
    {
        Manager::instance().requestUltralightUpdate();
    }
    return -1.0f; // call me every frame for smooth rendering
}

// Draw callback - called during X-Plane's 2D drawing phase
int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void *inRefcon)
{
    MainThreadDispatcher::instance().Drain(48, 512);

    if (!Manager::instance().isRendererReady())
    {
        return 1;
    }
    Manager::instance().requestUltralightRender(); // Render views asynchronously on Ultralight thread
    Manager::instance().updateAllApps();           // Upload staged bitmaps to textures
    Manager::instance().drawAllApps();             // Draw textured quads
    return 1;
}

bool Manager::isUltralightThread() const
{
    return std::this_thread::get_id() == ultralight_thread_id_;
}

void Manager::postToUltralightThread(std::function<void()> task, UltralightTaskPriority priority)
{
    if (!task)
    {
        return;
    }
    if (isUltralightThread())
    {
        task();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(ultralight_mutex_);
        if (priority == UltralightTaskPriority::High)
        {
            ultralight_high_priority_tasks_.push_back(std::move(task));
        }
        else
        {
            ultralight_normal_priority_tasks_.push_back(std::move(task));
        }
    }
    ultralight_cv_.notify_one();
}

void Manager::requestUltralightUpdate()
{
    if (!isRendererReady())
    {
        return;
    }
    pending_updates_.fetch_add(1, std::memory_order_relaxed);
    ultralight_cv_.notify_one();
}

void Manager::requestUltralightRender()
{
    if (!isRendererReady())
    {
        return;
    }
    pending_renders_.fetch_add(1, std::memory_order_relaxed);
    ultralight_cv_.notify_one();
}

void Manager::startUltralightThread()
{
    {
        std::lock_guard<std::mutex> lock(ultralight_mutex_);
        ultralight_thread_stop_ = false;
        ultralight_thread_started_ = false;
        ultralight_thread_exited_ = false;
        ultralight_high_priority_tasks_.clear();
        ultralight_normal_priority_tasks_.clear();
    }
    pending_updates_.store(0);
    pending_renders_.store(0);
    renderer_ready_.store(false);

    ultralight_thread_ = std::thread(&Manager::ultralightThreadMain, this);

    std::unique_lock<std::mutex> lock(ultralight_mutex_);
    ultralight_cv_.wait(lock, [this]
                        { return ultralight_thread_started_; });
}

void Manager::stopUltralightThread()
{
    if (!ultralight_thread_.joinable())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(ultralight_mutex_);
        ultralight_thread_stop_ = true;
        ultralight_high_priority_tasks_.clear();
        ultralight_normal_priority_tasks_.clear();
    }
    pending_updates_.store(0, std::memory_order_relaxed);
    pending_renders_.store(0, std::memory_order_relaxed);
    ultralight_cv_.notify_one();

    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(ultralight_mutex_);
            if (ultralight_cv_.wait_for(lock, std::chrono::milliseconds(2), [this]
                                        { return ultralight_thread_exited_; }))
            {
                break;
            }
        }
        MainThreadDispatcher::instance().Drain(64, 1024);
    }

    ultralight_thread_.join();
    ultralight_thread_id_ = std::thread::id();
}

void Manager::ultralightThreadMain()
{
    ultralight_thread_id_ = std::this_thread::get_id();

    renderer_ = Renderer::Create();
    renderer_ready_.store(renderer_.get() != nullptr);

    {
        std::lock_guard<std::mutex> lock(ultralight_mutex_);
        ultralight_thread_started_ = true;
    }
    ultralight_cv_.notify_all();

    while (true)
    {
        std::deque<std::function<void()>> high_priority_tasks;
        std::deque<std::function<void()>> normal_priority_tasks;
        uint32_t update_count = 0;
        uint32_t render_count = 0;

        {
            std::unique_lock<std::mutex> lock(ultralight_mutex_);
            ultralight_cv_.wait(lock, [this]
                                { return ultralight_thread_stop_ ||
                                         !ultralight_high_priority_tasks_.empty() ||
                                         !ultralight_normal_priority_tasks_.empty() ||
                                         pending_updates_.load(std::memory_order_relaxed) > 0 ||
                                         pending_renders_.load(std::memory_order_relaxed) > 0; });

            high_priority_tasks.swap(ultralight_high_priority_tasks_);
            normal_priority_tasks.swap(ultralight_normal_priority_tasks_);
            update_count = pending_updates_.exchange(0, std::memory_order_relaxed);
            render_count = pending_renders_.exchange(0, std::memory_order_relaxed);

            if (ultralight_thread_stop_ && high_priority_tasks.empty() && normal_priority_tasks.empty() &&
                update_count == 0 && render_count == 0)
            {
                break;
            }
        }

        for (auto &task : high_priority_tasks)
        {
            task();
        }

        for (auto &task : normal_priority_tasks)
        {
            task();
        }

        if (!renderer_)
        {
            continue;
        }

        if (update_count > 0)
        {
            renderer_->Update();
        }

        if (render_count > 0)
        {
            for (auto &[name, app] : apps_)
            {
                if (app)
                {
                    app->ForceRepaintOnUltralightThread();
                }
            }

            renderer_->Render();

            for (auto &[name, app] : apps_)
            {
                if (app)
                {
                    app->CaptureSurfacesOnUltralightThread();
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(ultralight_mutex_);
        ultralight_thread_started_ = false;
        ultralight_thread_exited_ = true;
    }
    ultralight_cv_.notify_all();

    renderer_ = nullptr;
    renderer_ready_.store(false);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void *params)
{
    MainThreadDispatcher::instance().Drain(24, 256);

    switch (msg)
    {
    case XPLM_MSG_PLANE_LOADED:
        if ((intptr_t)params != 0)
        {
            // It was not the user's plane. Ignore.
            return;
        }

        LogMsg("Plane loaded message received.");
        Manager::instance().initializeAllApps();
        break;

    case XPLM_MSG_PLANE_UNLOADED:
        if ((intptr_t)params != 0)
        {
            // It was not the user's plane. Ignore.
            return;
        }

        LogMsg("Plane unloaded message received.");
        Manager::instance().destroyAllApps();
        break;

    default:
        break;
    }
}

int Manager::initialize(char *out_name, char *out_sig, char *out_desc)
{
    LogMsg("Startup " VERSION);
    MainThreadDispatcher::instance().SetMainThread();

    // Initialization code here
    strcpy(out_name, name);
    strcpy(out_sig, signature);
    strcpy(out_desc, description);

    // Always use Unix-native paths on the Mac!
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

    char buffer[2048];
    XPLMGetSystemPath(buffer);
    std::string base_dir(buffer); // has trailing slash
    // get current xpl location
    char xpl_path[2048];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, xpl_path, nullptr, nullptr);
    std::string plugin_path(xpl_path);
    // go back up two levels to get to the plugin's root folder
    std::string current_plugin_dir = plugin_path.substr(0, plugin_path.find_last_of("/\\"));

    Manager::instance().setXpDir(base_dir);
    Manager::instance().setPluginDir(current_plugin_dir);
    Manager::instance().setOutputDir(base_dir + "Output/" + app_name);
    Manager::instance().setPrefPath(base_dir + "Output/preferences/" + app_name + ".prf");
    std::filesystem::create_directories(Manager::instance().getOutputDir());
    std::filesystem::create_directories(Manager::instance().getOutputDir() + "/cache");

    // Create Top Level Menu
    XPLMMenuID root_menu = XPLMFindPluginsMenu();
    menu_ = XPLMCreateMenu("SkyScript", root_menu, XPLMAppendMenuItem(root_menu, "SkyScript", NULL, 0), this->menuCB, nullptr);

    // Discover apps and create menu items
    discoverApps();

    LogMsg("XPluginStart done, xp_dir: '%s'", Manager::instance().getXpDir().c_str());

    // intialize Ultralight here
    Config config;
    // config.user_stylesheet = "body { background-color: #202020; color: #E0E0E0; }";
    config.cache_path = (output_dir + "/cache").c_str(); // For persistent sessions
    config.num_renderer_threads = ResolveUltralightRendererThreads();

    LogMsg("Ultralight renderer threads: %u (set SKYSCRIPT_UL_NUM_RENDERER_THREADS to override, 0=auto)",
           config.num_renderer_threads);

    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(plugin_dir.c_str()));
    Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

    startUltralightThread();

    // Register XP callbacks that enqueue update/render work on the Ultralight thread.
    XPLMRegisterFlightLoopCallback(update, 0.1, nullptr);

    // Register draw callback to draw all app windows during 2D phase
    XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    Manager::instance().initializeAllApps();

    return 1;
}

void Manager::enable()
{
    LogMsg("Plugin enabled");

    // Re-initialize apps if renderer exists (plugin was re-enabled after disable)
    if (isRendererReady())
    {
        LogMsg("Re-initializing apps after plugin re-enable");
        initializeAllApps();
    }
}

void Manager::disable()
{
    LogMsg("Plugin disabled - unregistering handlers and destroying all apps");
    UtilitiesBindings::Shutdown();
    HidBindings::Shutdown();
    destroyAllApps();
}

void Manager::stop()
{
    LogMsg("Plugin stopping - cleaning up resources");
    MainThreadDispatcher::instance().DrainAll();

    // Unregister callbacks
    XPLMUnregisterFlightLoopCallback(update, nullptr);
    XPLMUnregisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    // Release JS callback wrappers and HID handles while runtime is still alive.
    UtilitiesBindings::Shutdown();
    HidBindings::Shutdown();

    // Destroy all apps after callbacks are gone.
    destroyAllApps();

    LogMsg("Stopping Ultralight thread");
    stopUltralightThread();
    MainThreadDispatcher::instance().DrainAll();

    LogMsg("Plugin stopped");
}

void Manager::menuCB(void *menu_ref, void *item_ref)
{
    MainThreadDispatcher::instance().Drain(24, 256);

    const char *item_name = static_cast<const char *>(item_ref);
    if (item_name == nullptr)
    {
        LogMsg("Menu item selected: (null)");
        return;
    }

    // Find the app and toggle its window
    auto &apps = Manager::instance().apps_;
    auto it = apps.find(item_name);
    if (it != apps.end() && it->second)
    {
        it->second->Toggle();
        LogMsg("Toggled app: %s, visible: %d", item_name, it->second->IsVisible());
    }
    else
    {
        LogMsg("App not found: %s", item_name);
    }
}

void Manager::discoverApps()
{
    // find all folders under plugin_dir + "/apps"
    std::string apps_dir = plugin_dir + "/apps";

    if (!std::filesystem::exists(apps_dir))
    {
        LogMsg("Apps directory does not exist: %s", apps_dir.c_str());
        return;
    }

    // First pass: discover all apps except app_manager
    for (const auto &entry : std::filesystem::directory_iterator(apps_dir))
    {
        if (entry.is_directory())
        {
            std::string app_dir = entry.path().string();
            std::string app_name = entry.path().filename().string();

            // Skip app_manager for now - we'll add it last
            if (app_name == "app_manager")
            {
                continue;
            }

            LogMsg("Discovered app: %s, dir: %s", app_name.c_str(), app_dir.c_str());

            // Create App (but don't initialize yet - renderer not ready)
            auto app = std::make_unique<App>(app_name, app_dir);
            apps_.emplace(app_name, std::move(app));

            // Create menu item for this app (use the stored name from map key)
            auto &stored_app = apps_[app_name];
            XPLMAppendMenuItem(menu_, stored_app->GetDisplayName().c_str(),
                               (void *)stored_app->GetName().c_str(), 0);
        }
    }

    // Now add app_manager as a system app at the end with a separator
    std::string app_manager_dir = apps_dir + "/app_manager";
    if (std::filesystem::exists(app_manager_dir) && std::filesystem::is_directory(app_manager_dir))
    {
        LogMsg("Discovered system app: app_manager, dir: %s", app_manager_dir.c_str());

        // Create App
        auto app = std::make_unique<App>("app_manager", app_manager_dir);
        apps_.emplace("app_manager", std::move(app));

        // Add separator before app_manager
        XPLMAppendMenuSeparator(menu_);

        // Create menu item for app_manager
        auto &stored_app = apps_["app_manager"];
        XPLMAppendMenuItem(menu_, stored_app->GetDisplayName().c_str(),
                           (void *)stored_app->GetName().c_str(), 0);
    }
}

void Manager::initializeAllApps()
{
    // Initialize all discovered apps (renderer is now ready)
    for (auto &[name, app] : apps_)
    {
        if (app)
        {
            LogMsg("Initializing app: %s", name.c_str());
            app->Initialize();
        }
    }
}

void Manager::updateAllApps()
{
    // Update textures for all visible apps
    for (auto &[name, app] : apps_)
    {
        if (app)
        {
            app->ProcessPendingMainThreadWork();
        }
        if (app && app->IsVisible())
        {
            app->UpdateTexture();
        }
        // Also update inspector texture if visible
        if (app && app->IsInspectorVisible())
        {
            app->UpdateInspectorTexture();
        }
    }
}

void Manager::drawAllApps()
{
    // Draw all visible apps
    for (auto &[name, app] : apps_)
    {
        if (app && app->IsVisible())
        {
            app->Draw();
        }
        // Also draw inspector if visible
        if (app && app->IsInspectorVisible())
        {
            app->DrawInspector();
        }
    }
}

void Manager::forceRepaintAllApps()
{
    // Force all visible views to repaint
    for (auto &[name, app] : apps_)
    {
        if (app && (app->IsVisible() || app->IsInspectorVisible()))
        {
            app->ForceRepaint();
        }
    }
}

void Manager::destroyAllApps()
{
    LogMsg("Destroying all apps");
    for (auto &[name, app] : apps_)
    {
        if (app)
        {
            LogMsg("Destroying app: %s", name.c_str());
            app->Destroy();
        }
    }
    LogMsg("All apps destroyed");
}

// =========================================================================
// App Management API (for SkyScript JS bindings)
// =========================================================================

std::vector<std::string> Manager::getAppNames() const
{
    std::vector<std::string> names;
    for (const auto &[name, app] : apps_)
    {
        names.push_back(name);
    }
    return names;
}

bool Manager::reloadApp(const std::string &name)
{
    auto it = apps_.find(name);
    if (it != apps_.end() && it->second && it->second->IsInitialized())
    {
        LogMsg("Reloading app: %s", name.c_str());
        it->second->Reload();
        return true;
    }
    LogMsg("Failed to reload app (not found or not initialized): %s", name.c_str());
    return false;
}

bool Manager::openAppWindow(const std::string &name)
{
    auto it = apps_.find(name);
    if (it != apps_.end() && it->second)
    {
        // Initialize if not already done
        if (!it->second->IsInitialized())
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize();
        }

        LogMsg("Opening app window: %s", name.c_str());
        it->second->Show();
        return true;
    }
    LogMsg("Failed to open app window (not found or not initialized): %s", name.c_str());
    return false;
}

bool Manager::openAppInspector(const std::string &name)
{
    auto it = apps_.find(name);
    if (it != apps_.end() && it->second)
    {
        // Initialize if not already done
        if (!it->second->IsInitialized())
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize();
        }

        LogMsg("Opening inspector for app: %s", name.c_str());
        it->second->ShowInspector();
        return true;
    }
    LogMsg("Failed to open app inspector (not found or not initialized): %s", name.c_str());
    return false;
}
