#ifndef SKYSCRIPT_C_H
#define SKYSCRIPT_C_H

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

/* Returns a default-initialized config. */
SkyScriptAppConfig skyscript_default_config(void);

/* Lifecycle */
void skyscript_initialize(void);
void skyscript_shutdown(void);

/* Set the base directory for SkyScript assets (icons, sounds, etc.).
   If not called, defaults to <pluginDirectory>/assets.
   Must be called after skyscript_initialize and before creating app windows. */
void skyscript_set_assets_path(const char* path);

/* App discovery */
int skyscript_load_apps_from_directory(void);
void skyscript_reload_apps(void);

/* Window management */
SkyScriptApp skyscript_create_app_window(const char* name, const char* id, const SkyScriptAppConfig* config);
void skyscript_destroy_app_window(SkyScriptApp app);
void skyscript_destroy_all_app_windows(void);
int skyscript_get_app_window_count(void);

/* Active app */
SkyScriptApp skyscript_get_active_app(void);
void skyscript_set_active_app(SkyScriptApp app);
SkyScriptApp skyscript_find_app(const char* id);

/* App accessors */
const char* skyscript_app_get_name(SkyScriptApp app);
const char* skyscript_app_get_id(SkyScriptApp app);
int skyscript_app_is_visible(SkyScriptApp app);
void skyscript_app_show(SkyScriptApp app, const char* url);
void skyscript_app_hide(SkyScriptApp app);

#ifdef __cplusplus
}
#endif

#endif
