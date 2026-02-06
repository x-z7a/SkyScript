#include "path.h"

#include <filesystem>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

// Platform path separator exposed to JavaScript
#if IBM
static constexpr char kSeparator = '\\';
static const char* kSeparatorStr = "\\";
#else
static constexpr char kSeparator = '/';
static const char* kSeparatorStr = "/";
#endif

void PathBindings::Bind(JSObject& path) {
    path["join"]      = JSCallbackWithRetval(JS_Join);
    path["dirname"]   = JSCallbackWithRetval(JS_Dirname);
    path["basename"]  = JSCallbackWithRetval(JS_Basename);
    path["extname"]   = JSCallbackWithRetval(JS_Extname);
    path["normalize"] = JSCallbackWithRetval(JS_Normalize);
    path["sep"]       = JSValue(kSeparatorStr);
}

// ---------------------------------------------------------------------------
// SkyScript.path.join(...parts) → string
// ---------------------------------------------------------------------------
JSValue PathBindings::JS_Join(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty()) return JSValue("");

    fs::path result;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!args[i].IsString()) {
            LogMsg("PathBindings: join argument %d is not a string, skipping", i);
            continue;
        }
        String part = args[i].ToString();
        std::string s = part.utf8().data();

        if (i == 0) {
            result = s;
        } else {
            result /= s;
        }
    }

    std::string out = result.lexically_normal().string();

    // On Windows the canonical separator from std::filesystem is already '\'.
    // On POSIX it's '/'. Nothing extra to do.
    return JSValue(out.c_str());
}

// ---------------------------------------------------------------------------
// SkyScript.path.dirname(path) → string
// ---------------------------------------------------------------------------
JSValue PathBindings::JS_Dirname(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("PathBindings: dirname requires a string argument");
        return JSValue(".");
    }

    String pathStr = args[0].ToString();
    std::string s = pathStr.utf8().data();
    std::string dir = fs::path(s).parent_path().string();

    return JSValue(dir.empty() ? "." : dir.c_str());
}

// ---------------------------------------------------------------------------
// SkyScript.path.basename(path, ext?) → string
// ---------------------------------------------------------------------------
JSValue PathBindings::JS_Basename(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("PathBindings: basename requires a string argument");
        return JSValue("");
    }

    String pathStr = args[0].ToString();
    std::string s = pathStr.utf8().data();
    std::string name = fs::path(s).filename().string();

    // If a second argument is provided, strip that suffix
    if (args.size() >= 2 && args[1].IsString()) {
        String extStr = args[1].ToString();
        std::string ext = extStr.utf8().data();
        if (name.size() >= ext.size() &&
            name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
            name = name.substr(0, name.size() - ext.size());
        }
    }

    return JSValue(name.c_str());
}

// ---------------------------------------------------------------------------
// SkyScript.path.extname(path) → string   (includes the dot, e.g. ".txt")
// ---------------------------------------------------------------------------
JSValue PathBindings::JS_Extname(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("PathBindings: extname requires a string argument");
        return JSValue("");
    }

    String pathStr = args[0].ToString();
    std::string s = pathStr.utf8().data();
    std::string ext = fs::path(s).extension().string();

    return JSValue(ext.c_str());
}

// ---------------------------------------------------------------------------
// SkyScript.path.normalize(path) → string
// ---------------------------------------------------------------------------
JSValue PathBindings::JS_Normalize(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("PathBindings: normalize requires a string argument");
        return JSValue("");
    }

    String pathStr = args[0].ToString();
    std::string s = pathStr.utf8().data();
    std::string normalized = fs::path(s).lexically_normal().string();

    return JSValue(normalized.c_str());
}
