#include "xplm_bridge.h"

#include "app.h"
#include "dataref.h"
#include "config.h"
#include "json.hpp"
#include "path.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <XPLMDataAccess.h>

namespace {

using json = nlohmann::json;

// dump() throws on invalid UTF-8 by default, and a dataref or a file is not
// obliged to hold valid UTF-8. Replacing the bad bytes is better than failing
// the whole call.
std::string dumpJson(const json& value) {
    return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

// Floats go through their shortest decimal form, so 0.1f reaches the page as
// 0.1 rather than as the 0.10000000149011612 a straight widening produces.
double floatValue(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.7g", value);
    return std::strtod(buffer, nullptr);
}

bool isPathAllowed(const std::string& path) {
    std::string pluginDir = Path::getInstance()->pluginDirectory;
    if (pluginDir.empty()) {
        return false;
    }

    try {
        std::filesystem::path canonicalPlugin = std::filesystem::weakly_canonical(pluginDir);
        std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path);
        std::string pluginStr = canonicalPlugin.string();
        std::string pathStr = canonicalPath.string();

        // Path must start with the plugin directory
        if (pathStr.length() < pluginStr.length()) {
            return false;
        }
        return pathStr.compare(0, pluginStr.length(), pluginStr) == 0;
    } catch (...) {
        return false;
    }
}

void requirePathAllowed(const std::string& path) {
    if (!isPathAllowed(path)) {
        throw std::runtime_error("Access denied: path is outside the plugin directory");
    }
}

std::optional<std::string> handleGetDataref(const XplmRequest& req) {
    XPLMDataRef handle = XPLMFindDataRef(req.ref.c_str());
    if (!handle) {
        throw std::runtime_error("Dataref not found: " + req.ref);
    }

    XPLMDataTypeID types = XPLMGetDataRefTypes(handle);

    json result;
    if (types & xplmType_Double) {
        result = XPLMGetDatad(handle);
    } else if (types & xplmType_Float) {
        result = floatValue(XPLMGetDataf(handle));
    } else if (types & xplmType_Int) {
        result = XPLMGetDatai(handle);
    } else if (types & xplmType_Data) {
        std::string text;
        int size = XPLMGetDatab(handle, nullptr, 0, 0);
        if (size > 0) {
            std::string buf(static_cast<size_t>(size), '\0');
            XPLMGetDatab(handle, buf.data(), 0, size);
            // Byte datarefs are C strings padded with NULs.
            text = buf.substr(0, buf.find('\0'));
        }
        result = text;
    } else if (types & xplmType_FloatArray) {
        int size = XPLMGetDatavf(handle, nullptr, 0, 0);
        std::vector<float> values(size);
        XPLMGetDatavf(handle, values.data(), 0, size);
        result = json::array();
        for (float value : values) {
            result.push_back(floatValue(value));
        }
    } else if (types & xplmType_IntArray) {
        int size = XPLMGetDatavi(handle, nullptr, 0, 0);
        std::vector<int> values(size);
        XPLMGetDatavi(handle, values.data(), 0, size);
        result = values;
    } else {
        throw std::runtime_error("Unsupported dataref type for: " + req.ref);
    }

    return dumpJson(result);
}

std::optional<std::string> handleSetDataref(const XplmRequest& req) {
    Dataref* dr = Dataref::getInstance();

    if (req.valueType == "int") {
        dr->set<int>(req.ref.c_str(), std::stoi(req.value));
    } else if (req.valueType == "float") {
        dr->set<float>(req.ref.c_str(), std::stof(req.value));
    } else if (req.valueType == "string") {
        dr->set<std::string>(req.ref.c_str(), req.value);
    } else {
        // Default: try float
        dr->set<float>(req.ref.c_str(), std::stof(req.value));
    }

    return std::nullopt;
}

std::optional<std::string> handleExecuteCommand(const XplmRequest& req) {
    Dataref::getInstance()->executeCommand(req.ref.c_str());
    return std::nullopt;
}

std::optional<std::string> handleShowNotification(const XplmRequest& req) {
    if (!App::current) {
        throw std::runtime_error("No active app for notification");
    }

    NotificationOptions options = App::current->defaultNotificationOptions();

    if (!req.payload.empty() && req.payload != "null") {
        json payload = json::parse(req.payload, nullptr, false);
        if (payload.is_discarded() || !payload.is_object()) {
            throw std::runtime_error("Notification options must be a JSON object");
        }

        if (payload.contains("title") && payload["title"].is_string()) {
            options.title = payload["title"].get<std::string>();
        }
        if (payload.contains("body") && payload["body"].is_string()) {
            options.body = payload["body"].get<std::string>();
        }
        else if (payload.contains("message") && payload["message"].is_string()) {
            options.body = payload["message"].get<std::string>();
        }
        if (payload.contains("corner") && payload["corner"].is_string()) {
            options.corner = NotificationCornerFromString(payload["corner"].get<std::string>(), options.corner);
        }
        else if (payload.contains("location") && payload["location"].is_string()) {
            options.corner = NotificationCornerFromString(payload["location"].get<std::string>(), options.corner);
        }
        if (payload.contains("timeoutSeconds") && payload["timeoutSeconds"].is_number()) {
            options.timeoutSeconds = payload["timeoutSeconds"].get<float>();
        }
        else if (payload.contains("timeout") && payload["timeout"].is_number()) {
            options.timeoutSeconds = payload["timeout"].get<float>();
        }
        if (payload.contains("opacity") && payload["opacity"].is_number()) {
            options.opacity = payload["opacity"].get<float>();
        }
        if (payload.contains("slideSeconds") && payload["slideSeconds"].is_number()) {
            options.slideSeconds = payload["slideSeconds"].get<float>();
        }
        else if (payload.contains("slide_seconds") && payload["slide_seconds"].is_number()) {
            options.slideSeconds = payload["slide_seconds"].get<float>();
        }
        if (payload.contains("dismissible") && payload["dismissible"].is_boolean()) {
            options.dismissible = payload["dismissible"].get<bool>();
        }
        if (payload.contains("playSound") && payload["playSound"].is_boolean()) {
            options.playSound = payload["playSound"].get<bool>();
        }
        else if (payload.contains("sound") && payload["sound"].is_boolean()) {
            options.playSound = payload["sound"].get<bool>();
        }
    }

    App::current->showNotification(options);
    return std::nullopt;
}

std::optional<std::string> handleDismissNotification(const XplmRequest& req) {
    (void)req;
    if (App::current) {
        App::current->dismissNotification();
    }

    return std::nullopt;
}

std::optional<std::string> handleFsReadFile(const XplmRequest& req) {
    requirePathAllowed(req.ref);

    if (!std::filesystem::exists(req.ref)) {
        throw std::runtime_error("File not found: " + req.ref);
    }

    std::ifstream file(req.ref, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + req.ref);
    }

    std::ostringstream content;
    content << file.rdbuf();
    file.close();

    return dumpJson(content.str());
}

std::optional<std::string> handleFsWriteFile(const XplmRequest& req) {
    requirePathAllowed(req.ref);

    // Create parent directories if they don't exist
    std::filesystem::path filePath(req.ref);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(req.ref, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write file: " + req.ref);
    }

    file << req.value;
    file.close();

    return std::nullopt;
}

std::optional<std::string> handleFsListDir(const XplmRequest& req) {
    requirePathAllowed(req.ref);

    if (!std::filesystem::exists(req.ref) || !std::filesystem::is_directory(req.ref)) {
        throw std::runtime_error("Directory not found: " + req.ref);
    }

    json entries = json::array();
    for (const auto& entry : std::filesystem::directory_iterator(req.ref)) {
        entries.push_back(entry.path().filename().string());
    }

    return dumpJson(entries);
}

std::optional<std::string> handleFsExists(const XplmRequest& req) {
    requirePathAllowed(req.ref);
    return std::filesystem::exists(req.ref) ? "true" : "false";
}

// console.* from the page, forwarded by the injected shim. X-Plane does not
// surface the page's console to plugins, so the shim is the only way these
// reach Log.txt.
std::optional<std::string> handleConsoleLog(const XplmRequest& req) {
    if (!App::current || !App::current->config.console_logging) {
        return std::nullopt;
    }

    std::string level = req.valueType.empty() ? "LOG" : req.valueType;
    for (char& c : level) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    debug("[%s] [%s] %s\n", App::current->name.c_str(), level.c_str(), req.value.c_str());
    return std::nullopt;
}

XplmRequest parseRequest(const json& body) {
    if (!body.is_object()) {
        throw std::runtime_error("SkyScript request must be a JSON object");
    }

    // Every field arrives as a string; the injected API stringifies them so the
    // handlers keep the shapes they had when requests travelled as URL queries.
    auto field = [&body](const char* key) -> std::string {
        auto it = body.find(key);
        if (it == body.end() || it->is_null()) {
            return "";
        }
        return it->is_string() ? it->get<std::string>() : dumpJson(*it);
    };

    XplmRequest req;
    req.action = field("action");
    req.ref = field("ref");
    req.value = field("value");
    req.valueType = field("valueType");
    req.channel = field("channel");
    req.payload = field("payload");
    return req;
}

} // namespace

std::string XplmBridge::handleRequest(const char* text) {
    json envelope;
    try {
        json body = json::parse(text);
        // Tolerate the argument arriving double-encoded: the SDK describes it as
        // "serialised as a JSON string", which could mean either.
        if (body.is_string()) {
            body = json::parse(body.get<std::string>());
        }

        std::optional<std::string> value = dispatch(parseRequest(body));
        envelope["ok"] = true;
        if (value) {
            envelope["value"] = json::parse(*value);
        }
    } catch (const std::exception& e) {
        envelope = json::object();
        envelope["ok"] = false;
        envelope["error"] = e.what();
    }

    return dumpJson(envelope);
}

std::optional<std::string> XplmBridge::dispatch(const XplmRequest& req) {
    if (req.action == "getDataref") {
        return handleGetDataref(req);
    } else if (req.action == "setDataref") {
        return handleSetDataref(req);
    } else if (req.action == "executeCommand") {
        return handleExecuteCommand(req);
    } else if (req.action == "postMessage") {
        return handlePostMessage(req);
    } else if (req.action == "showNotification") {
        return handleShowNotification(req);
    } else if (req.action == "dismissNotification") {
        return handleDismissNotification(req);
    } else if (req.action == "fsReadFile") {
        return handleFsReadFile(req);
    } else if (req.action == "fsWriteFile") {
        return handleFsWriteFile(req);
    } else if (req.action == "fsListDir") {
        return handleFsListDir(req);
    } else if (req.action == "fsExists") {
        return handleFsExists(req);
    } else if (req.action == "consoleLog") {
        return handleConsoleLog(req);
    }

    throw std::runtime_error("Unknown action: " + req.action);
}

std::optional<std::string> XplmBridge::handlePostMessage(const XplmRequest& req) {
    MessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlersMutex);
        auto it = messageHandlers.find(req.channel);
        if (it == messageHandlers.end()) {
            throw std::runtime_error("No handler registered for channel: " + req.channel);
        }
        handler = it->second;
    }

    auto [response, error] = handler(req.payload);
    if (!error.empty()) {
        throw std::runtime_error(error);
    }

    // The handler answers with a JSON value. It used to be spliced into script
    // as-is, so an empty answer meant undefined; keep that.
    if (response.empty()) {
        return std::nullopt;
    }
    if (!json::accept(response)) {
        throw std::runtime_error("Handler for channel " + req.channel + " returned invalid JSON");
    }
    return response;
}

void XplmBridge::processPendingPosts() {
    std::vector<PendingPost> posts;
    {
        std::lock_guard<std::mutex> lock(postsMutex);
        posts.swap(pendingPosts);
    }

    for (const auto& post : posts) {
        deliverPost(post);
    }
}

void XplmBridge::onMessage(const std::string& channel, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    messageHandlers[channel] = handler;
}

void XplmBridge::postMessage(const std::string& channel, const std::string& payload) {
    // Queued rather than delivered here. This is public API: a plugin may call
    // it from a worker thread (a socket reader, a file loader), and
    // XPLMWindowInjectScript is main-thread only. processPendingPosts drains
    // this from the flight loop.
    if (!windowReady.load(std::memory_order_acquire)) {
        // With no window there is nothing to deliver to, and queueing would
        // only replay a stale burst later.
        return;
    }

    std::lock_guard<std::mutex> lock(postsMutex);
    if (pendingPosts.size() >= kMaxPendingPosts) {
        // A plugin posting faster than the sim draws must not grow this without
        // bound. Drop the oldest: newer state is the state worth showing.
        pendingPosts.erase(pendingPosts.begin());
    }
    pendingPosts.push_back({channel, payload});
}

void XplmBridge::deliverPost(const PendingPost& post) {
    if (!window) {
        return;
    }

    // The payload is parsed, not interpolated. Concatenating it into the
    // statement would make every caller's payload executable script: a plugin
    // passing anything that is not valid JSON - or JSON built from data it does
    // not control - would have it run instead of parsed. JSON.parse also fails
    // loudly on a malformed payload rather than producing a syntax error that
    // silently takes the whole statement with it.
    std::string js = "(function(){"
                     "  if (!window.skyscript || !window.skyscript._messageListeners) return;"
                     "  var listeners = window.skyscript._messageListeners[" + dumpJson(post.channel) + "];"
                     "  if (!listeners) return;"
                     "  var payload;"
                     "  try { payload = JSON.parse(" + dumpJson(post.payload) + "); }"
                     "  catch (e) { console.error('skyscript: postMessage payload is not valid JSON', e); return; }"
                     "  for (var i = 0; i < listeners.length; i++) {"
                     "    try { listeners[i](payload); } catch(e) { console.error('skyscript onMessage error:', e); }"
                     "  }"
                     "})();";

    XPLMWindowInjectScript(window, js.c_str());
}

void XplmBridge::setWindow(XPLMWindowID aWindow) {
    window = aWindow;
    windowReady.store(aWindow != nullptr, std::memory_order_release);
}

std::string XplmBridge::getInjectionScript(bool consoleLogging) {
    std::string script = R"(
(function() {
    if (window.skyscript && window.skyscript.xplm) return;

    window.skyscript = window.skyscript || {};
    window.skyscript.version = ')" VERSION R"(';
    window.skyscript.xplaneVersion = ')" + std::to_string(XPLANE_VERSION) + R"(';
    window.skyscript._messageListeners = window.skyscript._messageListeners || {};

    // X-Plane exposes our browser function as xplane.skyscript(request), which
    // returns a Promise of the {ok, value | error} envelope the plugin replies
    // with.
    function sendRequest(action, params) {
        if (typeof xplane === 'undefined' || typeof xplane.skyscript !== 'function') {
            return Promise.reject(new Error('SkyScript bridge is not available'));
        }

        var request = { action: action };
        for (var key in params) {
            if (params.hasOwnProperty(key) && params[key] !== undefined) {
                request[key] = String(params[key]);
            }
        }

        return Promise.resolve(xplane.skyscript(request)).then(function(response) {
            if (response && response.ok) {
                return response.value;
            }
            throw new Error(response && response.error ? response.error : 'No response from SkyScript');
        });
    }
    window.skyscript._sendRequest = sendRequest;

    window.skyscript.xplm = {
        getDataref: function(ref) {
            return sendRequest('getDataref', { ref: ref });
        },
        setDataref: function(ref, value, valueType) {
            var type = valueType;
            if (!type) {
                if (typeof value === 'number') {
                    type = Number.isInteger(value) ? 'int' : 'float';
                } else {
                    type = 'string';
                }
            }
            return sendRequest('setDataref', { ref: ref, value: String(value), valueType: type });
        },
        executeCommand: function(command) {
            return sendRequest('executeCommand', { ref: command });
        }
    };

    window.skyscript.postMessage = function(channel, payload) {
        var payloadStr = (payload === undefined) ? 'null' : JSON.stringify(payload);
        return sendRequest('postMessage', { channel: channel, payload: payloadStr });
    };

    window.skyscript.onMessage = function(channel, callback) {
        if (!window.skyscript._messageListeners[channel]) {
            window.skyscript._messageListeners[channel] = [];
        }
        window.skyscript._messageListeners[channel].push(callback);
    };

    window.skyscript.notification = {
        show: function(options) {
            var config = options || {};
            if (typeof config === 'string') {
                config = { body: config };
            }
            return sendRequest('showNotification', { payload: JSON.stringify(config) });
        },
        dismiss: function() {
            return sendRequest('dismissNotification', {});
        }
    };

    window.skyscript.notify = function(titleOrOptions, body, options) {
        if (typeof titleOrOptions === 'object' && titleOrOptions !== null) {
            return window.skyscript.notification.show(titleOrOptions);
        }

        var config = Object.assign({}, options || {});
        config.title = titleOrOptions === undefined ? '' : String(titleOrOptions);
        if (body !== undefined) {
            config.body = String(body);
        }
        return window.skyscript.notification.show(config);
    };

    // alert() would block the page with nobody to dismiss it; it has always
    // been shown as a notification instead.
    window.alert = function(message) {
        window.skyscript.notify('Alert', message === undefined ? '' : String(message));
    };

    window.skyscript.fs = {
        readFile: function(path) {
            return sendRequest('fsReadFile', { ref: path });
        },
        writeFile: function(path, content) {
            return sendRequest('fsWriteFile', { ref: path, value: content });
        },
        listDir: function(path) {
            return sendRequest('fsListDir', { ref: path });
        },
        exists: function(path) {
            return sendRequest('fsExists', { ref: path });
        }
    };
})();
)";

    if (consoleLogging) {
        script += R"(
(function() {
    if (!window.skyscript || window.skyscript._consoleForwarded) return;
    window.skyscript._consoleForwarded = true;

    function describe(value) {
        if (typeof value === 'string') return value;
        if (value instanceof Error) return value.stack || String(value);
        try {
            var text = JSON.stringify(value);
            return text === undefined ? String(value) : text;
        } catch (e) {
            return String(value);
        }
    }

    function forward(level, args) {
        var parts = [];
        for (var i = 0; i < args.length; i++) {
            parts.push(describe(args[i]));
        }
        // Not through sendRequest: a rejection here would be logged, and that
        // log would be forwarded, and so on.
        try {
            if (typeof xplane !== 'undefined' && typeof xplane.skyscript === 'function') {
                Promise.resolve(xplane.skyscript({ action: 'consoleLog', valueType: level, value: parts.join(' ') })).catch(function() {});
            }
        } catch (e) {}
    }

    ['log', 'info', 'warn', 'error', 'debug'].forEach(function(level) {
        var original = console[level];
        console[level] = function() {
            forward(level === 'info' || level === 'debug' ? 'log' : level, arguments);
            if (original) return original.apply(console, arguments);
        };
    });

    window.addEventListener('error', function(event) {
        forward('error', [event.message + ' (' + event.filename + ':' + event.lineno + ')']);
    });
    window.addEventListener('unhandledrejection', function(event) {
        forward('error', ['Unhandled promise rejection: ' + describe(event.reason)]);
    });
})();
)";
    }

    return script;
}
