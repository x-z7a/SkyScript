#ifndef SKYSCRIPT_C_H
#define SKYSCRIPT_C_H

/* ---- Export / import macros ---- */
#if defined(_WIN32) || defined(_WIN64)
    #ifdef SKYSCRIPT_BUILDING_DLL
        #define SKYSCRIPT_API __declspec(dllexport)
    #else
        #define SKYSCRIPT_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define SKYSCRIPT_API __attribute__((visibility("default")))
#else
    #define SKYSCRIPT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to an App window. */
typedef void* SkyScriptApp;

/* App configuration for creating windows. */
typedef struct {
    const char* homepage;        /* URL to load, or NULL for default */
    int audio_muted;             /* 1 = muted, 0 = unmuted */
    unsigned short minimum_width;
    unsigned char scroll_speed;
    const char* forced_language; /* BCP-47 language tag, or NULL */
    const char* user_agent;      /* Custom user-agent string, or NULL */
    int hide_addressbar;         /* 1 = hidden, 0 = shown */
    unsigned char framerate;     /* Target framerate (0 = default 15) */
    unsigned short width;        /* Initial window width (0 = default 1024) */
    unsigned short height;       /* Initial window height (0 = default 768) */
    int console_logging;         /* 1 = forward console.log to Log.txt */
} SkyScriptAppConfig;

typedef enum {
    SKYSCRIPT_NOTIFICATION_TOP_LEFT = 0,
    SKYSCRIPT_NOTIFICATION_TOP_RIGHT = 1,
    SKYSCRIPT_NOTIFICATION_BOTTOM_LEFT = 2,
    SKYSCRIPT_NOTIFICATION_BOTTOM_RIGHT = 3
} SkyScriptNotificationCorner;

typedef struct {
    const char* title;           /* Optional title, or NULL */
    const char* body;            /* Optional body text, or NULL */
    SkyScriptNotificationCorner corner;
    float timeout_seconds;       /* 0 = stay until dismissed */
    float opacity;               /* 0.15 to 0.95; 0 = default */
    float slide_seconds;         /* Slide-in/out duration; 0 = default */
    int dismissible;             /* 1 = show close affordance */
    int play_sound;              /* 1 = play bundled notification sound */
} SkyScriptNotificationOptions;

/* Returns a default-initialized config. */
SKYSCRIPT_API SkyScriptAppConfig skyscript_default_config(void);

/* Returns default notification options. */
SKYSCRIPT_API SkyScriptNotificationOptions skyscript_default_notification_options(void);

/* Lifecycle */
SKYSCRIPT_API void skyscript_initialize(void);
SKYSCRIPT_API void skyscript_shutdown(void);

/* Set the base directory for SkyScript assets (icons, sounds, etc.).
   If not called, defaults to <pluginDirectory>/assets.
   Must be called after skyscript_initialize and before creating app windows. */
SKYSCRIPT_API void skyscript_set_assets_path(const char* path);

/* Set the plugin root directory.
   If not called, the directory is auto-detected from XPLMGetPluginInfo.
   Must be called before skyscript_load_apps_from_directory. */
SKYSCRIPT_API void skyscript_set_plugin_path(const char* path);

/* App discovery */
SKYSCRIPT_API int skyscript_load_apps_from_directory(void);
SKYSCRIPT_API void skyscript_reload_apps(void);

/* Window management */
SKYSCRIPT_API SkyScriptApp skyscript_create_app_window(const char* name, const char* id, const SkyScriptAppConfig* config);
SKYSCRIPT_API void skyscript_destroy_app_window(SkyScriptApp app);
SKYSCRIPT_API void skyscript_destroy_all_app_windows(void);
SKYSCRIPT_API int skyscript_get_app_window_count(void);
SKYSCRIPT_API SkyScriptApp skyscript_get_app_window_at(int index);

/* Active app */
SKYSCRIPT_API SkyScriptApp skyscript_get_active_app(void);
SKYSCRIPT_API void skyscript_set_active_app(SkyScriptApp app);
SKYSCRIPT_API SkyScriptApp skyscript_find_app(const char* id);

/* App accessors */
SKYSCRIPT_API const char* skyscript_app_get_name(SkyScriptApp app);
SKYSCRIPT_API const char* skyscript_app_get_id(SkyScriptApp app);
SKYSCRIPT_API int skyscript_app_is_visible(SkyScriptApp app);
SKYSCRIPT_API void skyscript_app_show(SkyScriptApp app, const char* url);
SKYSCRIPT_API void skyscript_app_hide(SkyScriptApp app);

/* Notifications */
SKYSCRIPT_API void skyscript_app_show_notification(SkyScriptApp app, const SkyScriptNotificationOptions* options);
SKYSCRIPT_API void skyscript_app_dismiss_notification(SkyScriptApp app);

/* Message passing — JS ↔ Plugin communication.

   skyscript_app_on_message registers a handler for messages sent from
   JavaScript via window.skyscript.postMessage(channel, payload).

   The handler receives the JSON payload as a string and must return a
   JSON response string via *out_response (caller frees) or set
   *out_error to a non-NULL error string (caller frees) to reject
   the JS Promise.

   The handler is invoked on the thread that pumps SkyScript (the X-Plane
   flight loop) and must return promptly: it blocks that frame. A plugin whose
   answer comes from somewhere slow — a socket, a file, another process —
   should return a receipt here and deliver the result later with
   skyscript_app_post_message.

   skyscript_app_post_message pushes a JSON payload to all JS listeners
   registered via window.skyscript.onMessage(channel, callback).

   It may be called from any thread. The message is queued and delivered on the
   next flight loop, so a worker thread never touches CEF directly. The payload
   must be a JSON document: it is parsed in the page, not evaluated. */
typedef void (*SkyScriptMessageCallback)(
    const char* channel,
    const char* payload,
    char** out_response,
    char** out_error,
    void* user_data
);

SKYSCRIPT_API void skyscript_app_on_message(SkyScriptApp app, const char* channel, SkyScriptMessageCallback callback, void* user_data);
SKYSCRIPT_API void skyscript_app_post_message(SkyScriptApp app, const char* channel, const char* payload);

/* Logging */
SKYSCRIPT_API void skyscript_set_log_prefix(const char* prefix);

#ifdef __cplusplus
}
#endif

#endif
