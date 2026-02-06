#include "fs.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::string FsBindings::app_dir_;

void FsBindings::Bind(JSObject& fsObj, const std::string& app_dir) {
    app_dir_ = app_dir;

    fsObj["readFile"]  = JSCallbackWithRetval(JS_ReadFile);
    fsObj["writeFile"] = JSCallbackWithRetval(JS_WriteFile);
    fsObj["exists"]    = JSCallbackWithRetval(JS_Exists);
}

std::string FsBindings::ResolveSandboxedPath(const std::string& relative_path) {
    // Reject obviously empty paths
    if (relative_path.empty()) return "";

    // Build candidate: app_dir / relative_path
    fs::path base = fs::path(app_dir_);
    fs::path candidate = base / relative_path;

    // Normalise (resolve . and ..) then verify the result is still inside app_dir
    fs::path resolved = candidate.lexically_normal();
    fs::path base_normal = base.lexically_normal();

    // Ensure the resolved path starts with the sandbox root
    auto [root_end, _] = std::mismatch(base_normal.begin(), base_normal.end(), resolved.begin());
    if (root_end != base_normal.end()) {
        LogMsg("FsBindings: path escapes sandbox: %s", relative_path.c_str());
        return "";
    }

    return resolved.string();
}

// ---------------------------------------------------------------------------
// SkyScript.fs.readFile(path) → string | null
// ---------------------------------------------------------------------------
JSValue FsBindings::JS_ReadFile(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("FsBindings: readFile requires a string argument (path)");
        return JSValue();
    }

    String rel_str = args[0].ToString();
    std::string rel = rel_str.utf8().data();
    std::string path = ResolveSandboxedPath(rel);
    if (path.empty()) return JSValue();

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        LogMsg("FsBindings: readFile could not open: %s", path.c_str());
        return JSValue();
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return JSValue(ss.str().c_str());
}

// ---------------------------------------------------------------------------
// SkyScript.fs.writeFile(path, content) → boolean
// ---------------------------------------------------------------------------
JSValue FsBindings::JS_WriteFile(const JSObject& thisObject, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsString()) {
        LogMsg("FsBindings: writeFile requires (path: string, content: string)");
        return JSValue(false);
    }

    String rel_str = args[0].ToString();
    std::string rel = rel_str.utf8().data();
    std::string path = ResolveSandboxedPath(rel);
    if (path.empty()) return JSValue(false);

    String content_str = args[1].ToString();
    std::string content = content_str.utf8().data();

    // Create parent directories if they don't exist
    fs::path parent = fs::path(path).parent_path();
    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
        LogMsg("FsBindings: writeFile could not create directories: %s", ec.message().c_str());
        return JSValue(false);
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        LogMsg("FsBindings: writeFile could not open: %s", path.c_str());
        return JSValue(false);
    }

    file << content;
    return JSValue(file.good());
}

// ---------------------------------------------------------------------------
// SkyScript.fs.exists(path) → boolean
// ---------------------------------------------------------------------------
JSValue FsBindings::JS_Exists(const JSObject& thisObject, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) {
        LogMsg("FsBindings: exists requires a string argument (path)");
        return JSValue(false);
    }

    String rel_str = args[0].ToString();
    std::string rel = rel_str.utf8().data();
    std::string path = ResolveSandboxedPath(rel);
    if (path.empty()) return JSValue(false);

    return JSValue(fs::exists(path));
}
