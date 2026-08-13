#include "skyscript_c.h"
#include "skyscript.h"

#include <cstdlib>
#include <string>

namespace {
NotificationCorner toCppCorner(SkyScriptNotificationCorner corner) {
    switch (corner) {
        case SKYSCRIPT_NOTIFICATION_TOP_LEFT:
            return NotificationCorner::TopLeft;
        case SKYSCRIPT_NOTIFICATION_TOP_RIGHT:
            return NotificationCorner::TopRight;
        case SKYSCRIPT_NOTIFICATION_BOTTOM_LEFT:
            return NotificationCorner::BottomLeft;
        case SKYSCRIPT_NOTIFICATION_BOTTOM_RIGHT:
            return NotificationCorner::BottomRight;
    }

    return NotificationCorner::TopRight;
}

SkyScriptNotificationCorner toCCorner(NotificationCorner corner) {
    switch (corner) {
        case NotificationCorner::TopLeft:
            return SKYSCRIPT_NOTIFICATION_TOP_LEFT;
        case NotificationCorner::TopRight:
            return SKYSCRIPT_NOTIFICATION_TOP_RIGHT;
        case NotificationCorner::BottomLeft:
            return SKYSCRIPT_NOTIFICATION_BOTTOM_LEFT;
        case NotificationCorner::BottomRight:
            return SKYSCRIPT_NOTIFICATION_BOTTOM_RIGHT;
    }

    return SKYSCRIPT_NOTIFICATION_TOP_RIGHT;
}
}

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
    c.window_titleless = cpp.window_titleless ? 1 : 0;
    c.window_opacity = cpp.window_opacity;
    c.notification_corner = toCCorner(cpp.notification_corner);
    c.notification_timeout = cpp.notification_timeout;
    c.notification_opacity = cpp.notification_opacity;
    c.notification_slide_seconds = cpp.notification_slide_seconds;
    c.notification_sound = cpp.notification_sound ? 1 : 0;
    return c;
}

SkyScriptNotificationOptions skyscript_default_notification_options(void) {
    NotificationOptions cpp = Notification::defaultOptions();
    SkyScriptNotificationOptions c = {};
    c.title = "";
    c.body = "";
    c.corner = toCCorner(cpp.corner);
    c.timeout_seconds = cpp.timeoutSeconds;
    c.opacity = cpp.opacity;
    c.slide_seconds = cpp.slideSeconds;
    c.dismissible = cpp.dismissible ? 1 : 0;
    c.play_sound = cpp.playSound ? 1 : 0;
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

void skyscript_set_plugin_path(const char* path) {
    if (path) {
        SkyScript::setPluginPath(path);
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
        AppConfiguration cpp = App::defaultConfig();
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
        cpp.window_titleless = config->window_titleless != 0;
        // Zero is not an opacity anyone asks for, and a caller that zeroed the
        // struct did not choose it. Notification options already read it the
        // same way.
        if (config->window_opacity > 0.0f) {
            cpp.window_opacity = config->window_opacity;
        }
        cpp.notification_corner = toCppCorner(config->notification_corner);
        cpp.notification_timeout = config->notification_timeout;
        cpp.notification_opacity = config->notification_opacity;
        cpp.notification_slide_seconds = config->notification_slide_seconds;
        cpp.notification_sound = config->notification_sound != 0;
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

SkyScriptApp skyscript_get_app_window_at(int index) {
    const auto& apps = SkyScript::getAppWindows();
    if (index < 0 || index >= static_cast<int>(apps.size())) {
        return nullptr;
    }
    return static_cast<SkyScriptApp>(apps[index]);
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

void skyscript_app_show_notification(SkyScriptApp app, const SkyScriptNotificationOptions* options) {
    if (!app || !options) return;

    App *cppApp = static_cast<App*>(app);
    NotificationOptions cpp = cppApp->defaultNotificationOptions();
    cpp.title = options->title ? options->title : "";
    cpp.body = options->body ? options->body : "";
    cpp.corner = toCppCorner(options->corner);
    cpp.timeoutSeconds = options->timeout_seconds;
    cpp.opacity = options->opacity;
    cpp.slideSeconds = options->slide_seconds;
    cpp.dismissible = options->dismissible != 0;
    cpp.playSound = options->play_sound != 0;
    cppApp->showNotification(cpp);
}

void skyscript_app_dismiss_notification(SkyScriptApp app) {
    if (app) {
        static_cast<App*>(app)->dismissNotification();
    }
}

void skyscript_app_on_message(SkyScriptApp app, const char* channel, SkyScriptMessageCallback callback, void* user_data) {
    if (!app || !channel || !callback) return;

    std::string ch(channel);
    static_cast<App*>(app)->onMessage(ch, [callback, user_data, ch](const std::string& payload) -> std::pair<std::string, std::string> {
        char* out_response = nullptr;
        char* out_error = nullptr;

        callback(ch.c_str(), payload.c_str(), &out_response, &out_error, user_data);

        std::string response;
        std::string error;

        if (out_error) {
            error = out_error;
            free(out_error);
        }
        if (out_response) {
            response = out_response;
            free(out_response);
        }

        if (!error.empty()) {
            return {"", error};
        }
        if (response.empty()) {
            response = "null";
        }
        return {response, ""};
    });
}

void skyscript_app_post_message(SkyScriptApp app, const char* channel, const char* payload) {
    if (!app || !channel || !payload) return;
    static_cast<App*>(app)->postMessageToJS(channel, payload);
}

void skyscript_set_log_prefix(const char* prefix) {
    if (prefix) {
        SkyScript::setLogPrefix(prefix);
    }
}

}
