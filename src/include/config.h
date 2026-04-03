#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#define GL_BGRA GL_BGRA_EXT
#define GL_CLAMP_TO_EDGE 0x812F
#elif __linux__
#include <GL/gl.h>
#elif __GNUC__
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl.h>
#endif

#include <string>

inline std::string& _skyscript_log_prefix() {
    static std::string prefix = "[SkyScript]";
    return prefix;
}

#define set_brightness(value) glColor4f(value, value, value, 1.0f)
#define debug(format, ...)                                                           \
    {                                                                                \
        char buffer[1024];                                                           \
        snprintf(buffer, sizeof(buffer), "%s " format, _skyscript_log_prefix().c_str(), ##__VA_ARGS__); \
        XPLMDebugString(buffer);                                                     \
    }

#define PRODUCT_NAME "SkyScript"
#define FRIENDLY_NAME "SkyScript"
#ifdef SKYSCRIPT_VERSION
#define VERSION SKYSCRIPT_VERSION
#else
#define VERSION "0.0.0-dev"
#endif
#define VERSION_CHECK_URL "https://api.github.com/repos/x-z7a/skyscript/releases?per_page=1&page=1"
#define ALL_PLUGINS_DIRECTORY "/Resources/plugins/"
#define PLUGIN_DIRECTORY (ALL_PLUGINS_DIRECTORY PRODUCT_NAME)
#define BUNDLE_ID "com.x-z7a." PRODUCT_NAME
#define APPS_DIRECTORY "apps"

#define SCALE_IMAGES 1

#define REFRESH_INTERVAL_SECONDS_FAST 0.1
#define REFRESH_INTERVAL_SECONDS_SLOW 2.0
