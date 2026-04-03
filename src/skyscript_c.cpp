#include "skyscript_c.h"
#include "skyscript.h"

extern "C" {

SkyScriptAppConfig skyscript_default_config(void) {
    AppConfiguration cpp = App::defaultConfig();
    SkyScriptAppConfig c = {};
    c.homepage = "";
    c.audio_muted = cpp.audio_muted ? 1 : 0;
    c.minimum_width = cpp.minimum_width;
    c.scroll_speed = cpp.scroll_speed;
    c.forced_language = "";
    c.user_agent = "";
    c.hide_addressbar = cpp.hide_addressbar ? 1 : 0;
    c.framerate = cpp.framerate;
    c.width = cpp.width;
    c.height = cpp.height;
    c.console_logging = cpp.console_logging ? 1 : 0;
    return c;
}

void skyscript_initialize(void) {
    SkyScript::initialize();
}

void skyscript_shutdown(void) {
    SkyScript::shutdown();
}

void skyscript_set_assets_path(const char* path) {
    if (path) {
        SkyScript::setAssetsPath(path);
    }
}

int skyscript_load_apps_from_directory(void) {
    return SkyScript::loadAppsFromDirectory() ? 1 : 0;
}

void skyscript_reload_apps(void) {
    SkyScript::reloadApps();
}

SkyScriptApp skyscript_create_app_window(const char* name, const char* id, const SkyScriptAppConfig* config) {
    if (config) {
        AppConfiguration cpp;
        cpp.homepage = config->homepage ? config->homepage : "";
        cpp.audio_muted = config->audio_muted != 0;
        cpp.minimum_width = config->minimum_width;
        cpp.scroll_speed = config->scroll_speed;
        cpp.forced_language = config->forced_language ? config->forced_language : "";
        cpp.user_agent = config->user_agent ? config->user_agent : "";
        cpp.hide_addressbar = config->hide_addressbar != 0;
        cpp.framerate = config->framerate;
        cpp.width = config->width;
        cpp.height = config->height;
        cpp.console_logging = config->console_logging != 0;
        return static_cast<SkyScriptApp>(SkyScript::createAppWindow(name, id, cpp));
    }
    return static_cast<SkyScriptApp>(SkyScript::createAppWindow(name, id));
}

void skyscript_destroy_app_window(SkyScriptApp app) {
    if (app) {
        SkyScript::destroyAppWindow(static_cast<App*>(app));
    }
}

void skyscript_destroy_all_app_windows(void) {
    SkyScript::destroyAllAppWindows();
}

int skyscript_get_app_window_count(void) {
    return static_cast<int>(SkyScript::getAppWindows().size());
}

SkyScriptApp skyscript_get_active_app(void) {
    return static_cast<SkyScriptApp>(SkyScript::getActiveApp());
}

void skyscript_set_active_app(SkyScriptApp app) {
    if (app) {
        SkyScript::setActiveApp(static_cast<App*>(app));
    }
}

SkyScriptApp skyscript_find_app(const char* id) {
    if (!id) return nullptr;
    return static_cast<SkyScriptApp>(SkyScript::findApp(id));
}

const char* skyscript_app_get_name(SkyScriptApp app) {
    if (!app) return "";
    return static_cast<App*>(app)->name.c_str();
}

const char* skyscript_app_get_id(SkyScriptApp app) {
    if (!app) return "";
    return static_cast<App*>(app)->id.c_str();
}

int skyscript_app_is_visible(SkyScriptApp app) {
    if (!app) return 0;
    return static_cast<App*>(app)->visible ? 1 : 0;
}

void skyscript_app_show(SkyScriptApp app, const char* url) {
    if (app) {
        static_cast<App*>(app)->showBrowser(url ? url : "");
    }
}

void skyscript_app_hide(SkyScriptApp app) {
    if (app) {
        static_cast<App*>(app)->hideBrowser();
    }
}

void skyscript_set_log_prefix(const char* prefix) {
    if (prefix) {
        SkyScript::setLogPrefix(prefix);
    }
}

}
