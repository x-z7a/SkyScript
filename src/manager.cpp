#include "manager.h"
#include "ultralight_app.h"
#include "cef_manager.h"
#include "manifest_config.h"
#include "bindings/hid.h"
#include "bindings/utilities.h"
#include "third_party/picojson.h"

#include <fstream>

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
}

Manager::~Manager()
{
}

float update(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon)
{
    if (Manager::instance().renderer_)
    {
        Manager::instance().renderer_->Update();
    }
    // Drive CEF message loop on the main thread (no-op if CEF not initialized)
    CefManager::instance().DoMessageLoopWork();
    return -1.0f; // call me every frame for smooth rendering
}

// Draw callback — called during X-Plane's 2D drawing phase
int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void *inRefcon)
{
    if (!Manager::instance().renderer_)
    {
        return 1;
    }
    Manager::instance().forceRepaintAllApps();
    Manager::instance().renderer_->Render();
    Manager::instance().updateAllApps();
    Manager::instance().drawAllApps();
    return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, long msg, void *params)
{
    switch (msg)
    {
    case XPLM_MSG_PLANE_LOADED:
        if ((intptr_t)params != 0)
            return;
        LogMsg("Plane loaded message received.");
        Manager::instance().initializeAllApps();
        break;

    case XPLM_MSG_PLANE_UNLOADED:
        if ((intptr_t)params != 0)
            return;
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

    strcpy(out_name, name);
    strcpy(out_sig, signature);
    strcpy(out_desc, description);

    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

    char buffer[2048];
    XPLMGetSystemPath(buffer);
    std::string base_dir(buffer);

    char xpl_path[2048];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, xpl_path, nullptr, nullptr);
    std::string plugin_path(xpl_path);
    std::string current_plugin_dir = plugin_path.substr(0, plugin_path.find_last_of("/\\"));

    Manager::instance().setXpDir(base_dir);
    Manager::instance().setPluginDir(current_plugin_dir);
    Manager::instance().setOutputDir(base_dir + "Output/" + app_name);
    Manager::instance().setPrefPath(base_dir + "Output/preferences/" + app_name + ".prf");

    std::filesystem::create_directories(Manager::instance().getOutputDir());
    std::filesystem::create_directories(Manager::instance().getOutputDir() + "/cache");

    // Create top-level menu
    XPLMMenuID root_menu = XPLMFindPluginsMenu();
    menu_ = XPLMCreateMenu("SkyScript", root_menu,
                           XPLMAppendMenuItem(root_menu, "SkyScript", NULL, 0),
                           this->menuCB, nullptr);

    discoverApps();

    LogMsg("XPluginStart done, xp_dir: '%s'", Manager::instance().getXpDir().c_str());

    // Initialize Ultralight
    Config config;
    config.cache_path = (output_dir + "/cache").c_str();
    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(plugin_dir.c_str()));
    Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

    renderer_ = Renderer::Create();

    // Initialize CEF (no-op if SKYSCRIPT_CEF_ENABLED is not defined)
    std::string cef_cache = output_dir + "/cef_cache";
    CefManager::instance().Initialize(current_plugin_dir, cef_cache);

    XPLMRegisterFlightLoopCallback(update, 0.1, nullptr);
    XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    Manager::instance().initializeAllApps();

    return 1;
}

void Manager::enable()
{
    LogMsg("Plugin enabled");
    if (renderer_)
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
    // Do NOT call CefManager::Shutdown() here — CEF cannot be re-initialized.
    // CEF browsers are destroyed via destroyAllApps() → CefApp::Destroy().
}

void Manager::stop()
{
    LogMsg("Plugin stopping - cleaning up resources");

    XPLMUnregisterFlightLoopCallback(update, nullptr);
    XPLMUnregisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    UtilitiesBindings::Shutdown();
    HidBindings::Shutdown();

    destroyAllApps();

    // Shut down CEF before releasing the Ultralight renderer.
    // CefShutdown() is safe to call even if Initialize() was a no-op.
    CefManager::instance().Shutdown();

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

// ── App discovery ─────────────────────────────────────────────────────────────

void Manager::discoverApps()
{
    std::string apps_dir = plugin_dir + "/apps";

    if (!std::filesystem::exists(apps_dir))
    {
        LogMsg("Apps directory does not exist: %s", apps_dir.c_str());
        return;
    }

    auto create_app = [&](const std::string &app_name_str, const std::string &app_dir) {
        ManifestConfig config = ReadManifestConfig(app_dir + "/manifest.json");

        std::unique_ptr<AppBase> app;
        if (config.renderer_type == RendererType::Cef)
        {
#ifdef SKYSCRIPT_CEF_ENABLED
            // CefApp will be included when CEF support is compiled in.
            // For now fall through to Ultralight with a warning.
            LogMsg("[%s] CEF renderer requested but SKYSCRIPT_CEF_ENABLED build not yet complete — using Ultralight", app_name_str.c_str());
            app = std::make_unique<UltralightApp>(app_name_str, app_dir);
#else
            LogMsg("[%s] CEF renderer requested but SKYSCRIPT_CEF_ENABLED not defined — falling back to Ultralight", app_name_str.c_str());
            app = std::make_unique<UltralightApp>(app_name_str, app_dir);
#endif
        }
        else
        {
            app = std::make_unique<UltralightApp>(app_name_str, app_dir);
        }

        apps_.emplace(app_name_str, std::move(app));
        auto &stored = apps_[app_name_str];
        XPLMAppendMenuItem(menu_, stored->GetDisplayName().c_str(),
                           (void *)stored->GetName().c_str(), 0);
    };

    // First pass: all apps except app_manager
    for (const auto &entry : std::filesystem::directory_iterator(apps_dir))
    {
        if (!entry.is_directory())
            continue;

        std::string app_dir  = entry.path().string();
        std::string app_name_str = entry.path().filename().string();

        if (app_name_str == "app_manager")
            continue;

        LogMsg("Discovered app: %s, dir: %s", app_name_str.c_str(), app_dir.c_str());
        create_app(app_name_str, app_dir);
    }

    // app_manager last, with separator
    std::string mgr_dir = apps_dir + "/app_manager";
    if (std::filesystem::exists(mgr_dir) && std::filesystem::is_directory(mgr_dir))
    {
        LogMsg("Discovered system app: app_manager, dir: %s", mgr_dir.c_str());

        auto app = std::make_unique<UltralightApp>("app_manager", mgr_dir);
        apps_.emplace("app_manager", std::move(app));

        XPLMAppendMenuSeparator(menu_);

        auto &stored = apps_["app_manager"];
        XPLMAppendMenuItem(menu_, stored->GetDisplayName().c_str(),
                           (void *)stored->GetName().c_str(), 0);
    }
}

// ── App lifecycle ─────────────────────────────────────────────────────────────

void Manager::initializeAllApps()
{
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
    for (auto &[name, app] : apps_)
    {
        if (app && app->IsVisible())
            app->UpdateTexture();
        if (app && app->IsInspectorVisible())
            app->UpdateInspectorTexture();
    }
}

void Manager::drawAllApps()
{
    for (auto &[name, app] : apps_)
    {
        if (app && app->IsVisible())
            app->Draw();
        if (app && app->IsInspectorVisible())
            app->DrawInspector();
    }
}

void Manager::forceRepaintAllApps()
{
    for (auto &[name, app] : apps_)
    {
        if (app && (app->IsVisible() || app->IsInspectorVisible()))
            app->ForceRepaint();
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

// ── App Management API ────────────────────────────────────────────────────────

std::vector<std::string> Manager::getAppNames() const
{
    std::vector<std::string> names;
    for (const auto &[name, app] : apps_)
        names.push_back(name);
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
        if (!it->second->IsInitialized())
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize();
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
        if (!it->second->IsInitialized())
        {
            LogMsg("Initializing app on demand: %s", name.c_str());
            it->second->Initialize();
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

bool Manager::setAppRenderer(const std::string &name, const std::string &renderer)
{
    auto it = apps_.find(name);
    if (it == apps_.end())
    {
        LogMsg("setAppRenderer: app not found: %s", name.c_str());
        return false;
    }

    // Validate renderer string
    if (renderer != "ultralight" && renderer != "cef")
    {
        LogMsg("setAppRenderer: unknown renderer '%s' for app '%s'", renderer.c_str(), name.c_str());
        return false;
    }

    // Write "renderer" key to the app's manifest.json
    std::string app_dir = plugin_dir + "/apps/" + name;
    std::string manifest_file = app_dir + "/manifest.json";

    // Read existing manifest
    std::ifstream fin(manifest_file);
    std::string contents;
    if (fin.is_open())
    {
        contents.assign((std::istreambuf_iterator<char>(fin)),
                         std::istreambuf_iterator<char>());
        fin.close();
    }

    // Rewrite manifest.json with the new "renderer" key using picojson.
    {
        picojson::value root;
        std::string err = picojson::parse(root, contents);
        picojson::object obj;
        if (err.empty() && root.is_object())
            obj = root.get_object();

        obj["renderer"] = picojson::value(renderer);

        std::string new_contents = picojson::value(obj).serialize(true);
        std::ofstream fout(manifest_file);
        if (!fout.is_open())
        {
            LogMsg("setAppRenderer: cannot write manifest: %s", manifest_file.c_str());
            return false;
        }
        fout << new_contents;
        LogMsg("setAppRenderer: wrote renderer='%s' to %s", renderer.c_str(), manifest_file.c_str());
    }

    // Destroy and recreate the app with the new renderer
    bool was_visible = it->second->IsVisible();
    if (it->second->IsInitialized())
        it->second->Destroy();

    // Re-read manifest and recreate app
    ManifestConfig config = ReadManifestConfig(manifest_file);
    std::unique_ptr<AppBase> new_app;

    if (config.renderer_type == RendererType::Cef)
    {
#ifdef SKYSCRIPT_CEF_ENABLED
        LogMsg("setAppRenderer: falling through to Ultralight (CefApp not yet implemented)");
        new_app = std::make_unique<UltralightApp>(name, app_dir);
#else
        LogMsg("setAppRenderer: CEF not compiled in, using Ultralight");
        new_app = std::make_unique<UltralightApp>(name, app_dir);
#endif
    }
    else
    {
        new_app = std::make_unique<UltralightApp>(name, app_dir);
    }

    new_app->Initialize();
    if (was_visible)
        new_app->Show();

    it->second = std::move(new_app);
    LogMsg("setAppRenderer: app '%s' recreated with renderer '%s'", name.c_str(), renderer.c_str());
    return true;
}
