#include "manager.h"

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
    Manager::instance().renderer_->Update();
    return -1.0f; // call me every frame for smooth rendering
}

// Draw callback - called during X-Plane's 2D drawing phase
int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void *inRefcon)
{
    Manager::instance().forceRepaintAllApps(); // Mark all visible views as needing paint
    Manager::instance().renderer_->Render();   // Render views to bitmaps
    Manager::instance().updateAllApps();       // Upload bitmaps to textures
    Manager::instance().drawAllApps();         // Draw textured quads
    return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void *params)
{
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
    Manager::instance().setXpDir(base_dir);
    Manager::instance().setPluginDir(base_dir + "Resources/plugins/" + app_name);
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
    config.user_stylesheet = "body { background-color: #202020; color: #E0E0E0; }";
    config.cache_path = (output_dir + "/cache").c_str(); // For persistent sessions
    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(plugin_dir.c_str()));
    Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

    renderer_ = Renderer::Create();

    // register XP draw callbacks to call renderer_->Update() and renderer_->Render()
    XPLMRegisterFlightLoopCallback(update, 0.1, nullptr);

    // Register draw callback to draw all app windows during 2D phase
    XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    return 1;
}

void Manager::enable()
{
    LogMsg("Plugin enabled");

    // Re-initialize apps if renderer exists (plugin was re-enabled after disable)
    if (renderer_)
    {
        LogMsg("Re-initializing apps after plugin re-enable");
        initializeAllApps();
    }
}

void Manager::disable()
{
    LogMsg("Plugin disabled - destroying all apps");
    destroyAllApps();
}

void Manager::stop()
{
    LogMsg("Plugin stopping - cleaning up resources");

    // Destroy all apps first
    destroyAllApps();

    // Unregister callbacks
    XPLMUnregisterFlightLoopCallback(update, nullptr);
    XPLMUnregisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    // Release the renderer
    if (renderer_)
    {
        LogMsg("Releasing Ultralight renderer");
        renderer_ = nullptr;
    }

    LogMsg("Plugin stopped");
}

void Manager::menuCB(void *menu_ref, void *item_ref)
{
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

    // First pass: discover all user apps except app_manager
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
            XPLMAppendMenuItem(menu_, stored_app->GetName().c_str(),
                               (void *)stored_app->GetName().c_str(), 0);
        }
    }

    // Add default URL-based apps (between user apps and system app)
    addDefaultApps();

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
        XPLMAppendMenuItem(menu_, stored_app->GetName().c_str(),
                           (void *)stored_app->GetName().c_str(), 0);
    }
}

void Manager::addDefaultApps()
{
    // Define default URL-based apps here
    // These are web pages that can be loaded as apps
    struct DefaultApp
    {
        std::string name;
        std::string url;
    };

    std::vector<DefaultApp> default_apps = {
        // Add your default URL apps here, for example:
        // {"X-Plane Forum", "https://forums.x-plane.org"},
        // {"Navigraph Charts", "https://charts.navigraph.com"},
        // {"SimBrief", "https://www.simbrief.com"},
        {"SkyScript Docs", "https://x-z7a.github.io/SkyScript/"},
    };

    if (default_apps.empty())
    {
        return; // No default apps configured
    }

    // Add separator before default apps
    XPLMAppendMenuSeparator(menu_);

    for (const auto &def_app : default_apps)
    {
        LogMsg("Adding default app: %s, url: %s", def_app.name.c_str(), def_app.url.c_str());

        auto app = std::make_unique<App>(def_app.name, def_app.url, AppType::URL);
        apps_.emplace(def_app.name, std::move(app));

        auto &stored_app = apps_[def_app.name];
        XPLMAppendMenuItem(menu_, stored_app->GetName().c_str(),
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
            app->Initialize(renderer_);
        }
    }
}

void Manager::updateAllApps()
{
    // Update textures for all visible apps
    for (auto &[name, app] : apps_)
    {
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
        if (app && app->IsInitialized())
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
        if (!it->second->IsInitialized() && renderer_)
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize(renderer_);
        }

        if (it->second->IsInitialized())
        {
            LogMsg("Opening app window: %s", name.c_str());
            it->second->Show();
            return true;
        }
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
        if (!it->second->IsInitialized() && renderer_)
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize(renderer_);
        }

        if (it->second->IsInitialized())
        {
            LogMsg("Opening inspector for app: %s", name.c_str());
            it->second->ShowInspector();
            return true;
        }
    }
    LogMsg("Failed to open app inspector (not found or not initialized): %s", name.c_str());
    return false;
}