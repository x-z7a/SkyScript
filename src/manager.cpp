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

    XPLMAppendMenuItem(menu_, "App Manager", (void *)"App Manager", 0);
    XPLMAppendMenuSeparator(menu_);

    LogMsg("XPluginStart done, xp_dir: '%s'", Manager::instance().getXpDir().c_str());

    // intialize Ultralight here
    Config config;
    config.user_stylesheet = "body { background-color: #202020; color: #E0E0E0; }";
    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(plugin_dir.c_str()));
    Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

    renderer_ = Renderer::Create();

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
        LogMsg("Menu item selected: %p, %p, name: (null)", menu_ref, item_ref);
        return;
    }

    if (strcmp(item_name, "App Manager") == 0)
    {
        // TODO: load app manager window
        LogMsg("App Manager selected.");
    }
    else
    {
        LogMsg("Menu item selected: %p, %p, name: %s", menu_ref, item_ref, item_name);
    }
}
