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

        // case XPLM_MSG_PLANE_UNLOADED:
        //     if ((intptr_t)params != 0) {
        //         // It was not the user's plane. Ignore.
        //         return;
        //     }

        //     Dataref::getInstance()->executeCommand("AviTab/Home");
        //     AppState::getInstance()->deinitialize();
        //     break;

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
    std::filesystem::create_directory(Manager::instance().getOutputDir());

    // Create Top Level Menu
    XPLMMenuID root_menu = XPLMFindPluginsMenu();
    menu_ = XPLMCreateMenu("SkyScript", root_menu, XPLMAppendMenuItem(root_menu, "SkyScript", NULL, 0), this->menuCB, nullptr);

    // Discover apps and create menu items
    discoverApps();

    LogMsg("XPluginStart done, xp_dir: '%s'", Manager::instance().getXpDir().c_str());

    // intialize Ultralight here
    Config config;
    config.user_stylesheet = "body { background-color: #202020; color: #E0E0E0; }";
    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(plugin_dir.c_str()));
    Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

    renderer_ = Renderer::Create();

    // register XP draw callbacks to call renderer_->Update() and renderer_->Render()
    XPLMRegisterFlightLoopCallback(update, 0.1, nullptr);
    
    // Register draw callback to draw all app windows during 2D phase
    XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Window, 0, nullptr);

    // RefPtr<View> view = renderer_->CreateView(800, 600, ViewConfig(), nullptr);

    // // multiple line of js code to test
    // String scripts = R"(
    //     function add(a, b) {
    //         return a + b;
    //     }
    //     add(5, 7);
    // )";

    // String result = view->EvaluateScript(scripts);
    // LogMsg("FFFFFFF!!!!!!!!!!%s", result.utf8().data());

    // scripts = R"(
    //     function add(a, b) {
    //         return a + b;
    //     }
    //     add(5, "7");
    // )";
    // result = view->EvaluateScript(scripts);
    // LogMsg("FFFFFFF!!!!!!!!!!%s", result.utf8().data());

    // RefPtr<JSContext> context = view->LockJSContext();
    // SetJSContext(context->ctx());
    // JSObject global = JSGlobalObject();
    // global["test"] = BindJSCallbackWithRetval(&Manager::test);
    // scripts = R"(
    //     test();
    // )";
    // result = view->EvaluateScript(scripts);
    // LogMsg("TEST!!!!!!!!!!%s", result.utf8().data());

    return 1;
}

void Manager::enable()
{
    LogMsg("Plugin enabled");
}
void Manager::disable()
{
    LogMsg("Plugin disabled");
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

    for (const auto &entry : std::filesystem::directory_iterator(apps_dir))
    {
        if (entry.is_directory())
        {
            std::string app_dir = entry.path().string();
            std::string app_name = entry.path().filename().string();
            LogMsg("Discovered app: %s, dir: %s", app_name.c_str(), app_dir.c_str());

            // Create App (but don't initialize yet - renderer not ready)
            auto app = std::make_unique<App>(app_name, app_dir);
            apps_.emplace(app_name, std::move(app));
            
            // Create menu item for this app (use the stored name from map key)
            auto& stored_app = apps_[app_name];
            XPLMAppendMenuItem(menu_, stored_app->GetName().c_str(), 
                              (void*)stored_app->GetName().c_str(), 0);
        }
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
    }
}

void Manager::forceRepaintAllApps()
{
    // Force all visible views to repaint
    for (auto &[name, app] : apps_)
    {
        if (app && app->IsVisible())
        {
            app->ForceRepaint();
        }
    }
}