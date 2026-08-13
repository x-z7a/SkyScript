#include "xplm_bridge.h"

#include "app.h"
#include "dataref.h"
#include "config.h"
#include "json.hpp"
#include "path.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <include/cef_parser.h>
#include <XPLMDataAccess.h>

void XplmBridge::enqueueRequest(const XplmRequest& req) {
    std::lock_guard<std::mutex> lock(mutex);
    pendingRequests.push_back(req);
}

void XplmBridge::processPendingRequests() {
    std::vector<XplmRequest> requests;
    {
        std::lock_guard<std::mutex> lock(mutex);
        requests.swap(pendingRequests);
    }

    for (const auto& req : requests) {
        if (req.action == "getDataref") {
            handleGetDataref(req);
        } else if (req.action == "setDataref") {
            handleSetDataref(req);
        } else if (req.action == "executeCommand") {
            handleExecuteCommand(req);
        } else if (req.action == "postMessage") {
            handlePostMessage(req);
        } else if (req.action == "showNotification") {
            handleShowNotification(req);
        } else if (req.action == "dismissNotification") {
            handleDismissNotification(req);
        } else if (req.action == "fsReadFile") {
            handleFsReadFile(req);
        } else if (req.action == "fsWriteFile") {
            handleFsWriteFile(req);
        } else if (req.action == "fsListDir") {
            handleFsListDir(req);
        } else if (req.action == "fsExists") {
            handleFsExists(req);
        } else {
            respond(req, "\"Unknown action: " + req.action + "\"", true);
        }
    }

    // Plugin-to-JS posts are drained here too, so they reach CEF on the same
    // thread everything else does. Draining after the requests lets a message
    // handler answer on this frame rather than the next one.
    std::vector<PendingPost> posts;
    {
        std::lock_guard<std::mutex> lock(postsMutex);
        posts.swap(pendingPosts);
    }

    for (const auto& post : posts) {
        deliverPost(post);
    }
}

void XplmBridge::handleGetDataref(const XplmRequest& req) {
    XPLMDataRef handle = XPLMFindDataRef(req.ref.c_str());
    if (!handle) {
        respond(req, "\"Dataref not found: " + req.ref + "\"", true);
        return;
    }

    XPLMDataTypeID types = XPLMGetDataRefTypes(handle);

    std::ostringstream result;
    if (types & xplmType_Double) {
        result << std::fixed << XPLMGetDatad(handle);
    } else if (types & xplmType_Float) {
        result << std::fixed << XPLMGetDataf(handle);
    } else if (types & xplmType_Int) {
        result << XPLMGetDatai(handle);
    } else if (types & xplmType_Data) {
        int size = XPLMGetDatab(handle, nullptr, 0, 0);
        if (size > 0) {
            std::string buf(static_cast<size_t>(size), '\0');
            XPLMGetDatab(handle, buf.data(), 0, size);
            // Escape for JS string literal
            std::string escaped;
            for (char c : buf) {
                if (c == '\\') escaped += "\\\\";
                else if (c == '"') escaped += "\\\"";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\r') escaped += "\\r";
                else if (c == '\0') break;
                else escaped += c;
            }
            result << "\"" << escaped << "\"";
        } else {
            result << "\"\"";
        }
    } else if (types & xplmType_FloatArray) {
        int size = XPLMGetDatavf(handle, nullptr, 0, 0);
        std::vector<float> values(size);
        XPLMGetDatavf(handle, values.data(), 0, size);
        result << "[";
        for (int i = 0; i < size; i++) {
            if (i > 0) result << ",";
            result << std::fixed << values[i];
        }
        result << "]";
    } else if (types & xplmType_IntArray) {
        int size = XPLMGetDatavi(handle, nullptr, 0, 0);
        std::vector<int> values(size);
        XPLMGetDatavi(handle, values.data(), 0, size);
        result << "[";
        for (int i = 0; i < size; i++) {
            if (i > 0) result << ",";
            result << values[i];
        }
        result << "]";
    } else {
        respond(req, "\"Unsupported dataref type for: " + req.ref + "\"", true);
        return;
    }

    respond(req, result.str());
}

void XplmBridge::handleSetDataref(const XplmRequest& req) {
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

    respond(req, "undefined");
}

void XplmBridge::handleExecuteCommand(const XplmRequest& req) {
    Dataref::getInstance()->executeCommand(req.ref.c_str());
    respond(req, "undefined");
}

void XplmBridge::handlePostMessage(const XplmRequest& req) {
    MessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlersMutex);
        auto it = messageHandlers.find(req.channel);
        if (it == messageHandlers.end()) {
            respond(req, "\"No handler registered for channel: " + escapeJsString(req.channel) + "\"", true);
            return;
        }
        handler = it->second;
    }

    auto [response, error] = handler(req.payload);
    if (!error.empty()) {
        respond(req, "\"" + escapeJsString(error) + "\"", true);
    } else {
        // response is already a JSON value string (could be object, array, string, number, etc.)
        respond(req, response);
    }
}

void XplmBridge::handleShowNotification(const XplmRequest& req) {
    if (!App::current) {
        respond(req, "\"No active app for notification\"", true);
        return;
    }

    NotificationOptions options = App::current->defaultNotificationOptions();

    if (!req.payload.empty() && req.payload != "null") {
        nlohmann::json json = nlohmann::json::parse(req.payload, nullptr, false);
        if (json.is_discarded() || !json.is_object()) {
            respond(req, "\"Notification options must be a JSON object\"", true);
            return;
        }

        if (json.contains("title") && json["title"].is_string()) {
            options.title = json["title"].get<std::string>();
        }
        if (json.contains("body") && json["body"].is_string()) {
            options.body = json["body"].get<std::string>();
        }
        else if (json.contains("message") && json["message"].is_string()) {
            options.body = json["message"].get<std::string>();
        }
        if (json.contains("corner") && json["corner"].is_string()) {
            options.corner = NotificationCornerFromString(json["corner"].get<std::string>(), options.corner);
        }
        else if (json.contains("location") && json["location"].is_string()) {
            options.corner = NotificationCornerFromString(json["location"].get<std::string>(), options.corner);
        }
        if (json.contains("timeoutSeconds") && json["timeoutSeconds"].is_number()) {
            options.timeoutSeconds = json["timeoutSeconds"].get<float>();
        }
        else if (json.contains("timeout") && json["timeout"].is_number()) {
            options.timeoutSeconds = json["timeout"].get<float>();
        }
        if (json.contains("opacity") && json["opacity"].is_number()) {
            options.opacity = json["opacity"].get<float>();
        }
        if (json.contains("slideSeconds") && json["slideSeconds"].is_number()) {
            options.slideSeconds = json["slideSeconds"].get<float>();
        }
        else if (json.contains("slide_seconds") && json["slide_seconds"].is_number()) {
            options.slideSeconds = json["slide_seconds"].get<float>();
        }
        if (json.contains("dismissible") && json["dismissible"].is_boolean()) {
            options.dismissible = json["dismissible"].get<bool>();
        }
        if (json.contains("playSound") && json["playSound"].is_boolean()) {
            options.playSound = json["playSound"].get<bool>();
        }
        else if (json.contains("sound") && json["sound"].is_boolean()) {
            options.playSound = json["sound"].get<bool>();
        }
    }

    App::current->showNotification(options);
    respond(req, "undefined");
}

void XplmBridge::handleDismissNotification(const XplmRequest& req) {
    if (App::current) {
        App::current->dismissNotification();
    }

    respond(req, "undefined");
}

void XplmBridge::handleFsReadFile(const XplmRequest& req) {
    if (!isPathAllowed(req.ref)) {
        respond(req, "\"Access denied: path is outside the plugin directory\"", true);
        return;
    }

    if (!std::filesystem::exists(req.ref)) {
        respond(req, "\"File not found: " + escapeJsString(req.ref) + "\"", true);
        return;
    }

    std::ifstream file(req.ref, std::ios::binary);
    if (!file.is_open()) {
        respond(req, "\"Cannot open file: " + escapeJsString(req.ref) + "\"", true);
        return;
    }

    std::ostringstream content;
    content << file.rdbuf();
    file.close();

    respond(req, "\"" + escapeJsString(content.str()) + "\"");
}

void XplmBridge::handleFsWriteFile(const XplmRequest& req) {
    if (!isPathAllowed(req.ref)) {
        respond(req, "\"Access denied: path is outside the plugin directory\"", true);
        return;
    }

    // Create parent directories if they don't exist
    std::filesystem::path filePath(req.ref);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(req.ref, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        respond(req, "\"Cannot write file: " + escapeJsString(req.ref) + "\"", true);
        return;
    }

    file << req.value;
    file.close();

    respond(req, "undefined");
}

void XplmBridge::handleFsListDir(const XplmRequest& req) {
    if (!isPathAllowed(req.ref)) {
        respond(req, "\"Access denied: path is outside the plugin directory\"", true);
        return;
    }

    if (!std::filesystem::exists(req.ref) || !std::filesystem::is_directory(req.ref)) {
        respond(req, "\"Directory not found: " + escapeJsString(req.ref) + "\"", true);
        return;
    }

    std::ostringstream result;
    result << "[";
    bool first = true;
    for (const auto& entry : std::filesystem::directory_iterator(req.ref)) {
        if (!first) result << ",";
        result << "\"" << escapeJsString(entry.path().filename().string()) << "\"";
        first = false;
    }
    result << "]";

    respond(req, result.str());
}

void XplmBridge::handleFsExists(const XplmRequest& req) {
    if (!isPathAllowed(req.ref)) {
        respond(req, "\"Access denied: path is outside the plugin directory\"", true);
        return;
    }

    respond(req, std::filesystem::exists(req.ref) ? "true" : "false");
}

void XplmBridge::onMessage(const std::string& channel, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    messageHandlers[channel] = handler;
}

void XplmBridge::postMessage(const std::string& channel, const std::string& payload) {
    // Queued rather than delivered here. This is public API: a plugin may call
    // it from a worker thread (a socket reader, a file loader), while
    // lastBrowser is written on the main thread from OnAfterCreated and CEF
    // wants ExecuteJavaScript on the thread CefDoMessageLoopWork runs on.
    // Touching either from a caller's thread is a data race on a refcounted
    // pointer. processPendingRequests drains this on the main thread, the same
    // way requests coming the other way are already handled.
    if (!browserReady.load(std::memory_order_acquire)) {
        // Matches the previous behaviour: with no browser there is nothing to
        // deliver to, and queueing would only replay a stale burst later.
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
    if (!lastBrowser || !lastBrowser->GetMainFrame()) {
        return;
    }

    // The payload is parsed, not interpolated. Concatenating it into the
    // statement made every caller's payload executable script: a plugin passing
    // anything that is not valid JSON - or JSON built from data it does not
    // control - would have it run instead of parsed. JSON.parse also fails
    // loudly on a malformed payload rather than producing a syntax error that
    // silently takes the whole statement with it.
    std::string js = "(function(){"
                     "  if (!window.skyscript || !window.skyscript._messageListeners) return;"
                     "  var listeners = window.skyscript._messageListeners['" + escapeJsString(post.channel) + "'];"
                     "  if (!listeners) return;"
                     "  var payload;"
                     "  try { payload = JSON.parse('" + escapeJsString(post.payload) + "'); }"
                     "  catch (e) { console.error('skyscript: postMessage payload is not valid JSON', e); return; }"
                     "  for (var i = 0; i < listeners.length; i++) {"
                     "    try { listeners[i](payload); } catch(e) { console.error('skyscript onMessage error:', e); }"
                     "  }"
                     "})();";

    lastBrowser->GetMainFrame()->ExecuteJavaScript(js, lastBrowser->GetMainFrame()->GetURL(), 0);
}

void XplmBridge::setBrowser(CefRefPtr<CefBrowser> browser) {
    lastBrowser = browser;
    browserReady.store(browser != nullptr, std::memory_order_release);
}

std::string XplmBridge::escapeJsString(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '\\') escaped += "\\\\";
        else if (c == '"') escaped += "\\\"";
        else if (c == '\'') escaped += "\\'";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\0') break;
        else escaped += c;
    }
    return escaped;
}

bool XplmBridge::isPathAllowed(const std::string& path) const {
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

void XplmBridge::respond(const XplmRequest& req, const std::string& jsValue, bool isError) {
    if (!req.browser || !req.browser->GetMainFrame()) {
        return;
    }

    std::string js;
    if (isError) {
        js = "window.skyscript._reject(" + std::to_string(req.callbackId) + ", " + jsValue + ");";
    } else {
        js = "window.skyscript._resolve(" + std::to_string(req.callbackId) + ", " + jsValue + ");";
    }

    req.browser->GetMainFrame()->ExecuteJavaScript(js, req.browser->GetMainFrame()->GetURL(), 0);
}

XplmRequest XplmBridge::parseRequestUrl(const std::string& url, CefRefPtr<CefBrowser> browser) {
    XplmRequest req;
    req.browser = browser;
    req.callbackId = 0;

    // URL formats:
    //   skyscript://xplm/<action>?ref=...&callbackId=...&value=...&valueType=...
    //   skyscript://message/postMessage?channel=...&payload=...&callbackId=...
    //   skyscript://notification/<action>?payload=...&callbackId=...
    //   skyscript://fs/<action>?ref=...&callbackId=...&value=...
    std::string remainder = url.substr(std::string("skyscript://").length());

    std::string pathPart = remainder;
    std::string queryPart;
    auto qpos = remainder.find('?');
    if (qpos != std::string::npos) {
        pathPart = remainder.substr(0, qpos);
        queryPart = remainder.substr(qpos + 1);
    }

    // Parse query parameters first
    if (!queryPart.empty()) {
        std::istringstream qs(queryPart);
        std::string pair;
        while (std::getline(qs, pair, '&')) {
            auto eq = pair.find('=');
            if (eq == std::string::npos) continue;
            std::string key = pair.substr(0, eq);
            std::string value = CefURIDecode(pair.substr(eq + 1), true, static_cast<cef_uri_unescape_rule_t>(UU_SPACES | UU_PATH_SEPARATORS | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | UU_REPLACE_PLUS_WITH_SPACE)).ToString();

            if (key == "ref") req.ref = value;
            else if (key == "callbackId") req.callbackId = std::stoi(value);
            else if (key == "value") req.value = value;
            else if (key == "valueType") req.valueType = value;
            else if (key == "channel") req.channel = value;
            else if (key == "payload") req.payload = value;
        }
    }

    // Determine action from path
    if (pathPart.starts_with("xplm/")) {
        req.action = pathPart.substr(std::string("xplm/").length());
    } else if (pathPart.starts_with("message/")) {
        req.action = pathPart.substr(std::string("message/").length());
    } else if (pathPart.starts_with("notification/")) {
        req.action = pathPart.substr(std::string("notification/").length());
    } else if (pathPart.starts_with("fs/")) {
        req.action = pathPart.substr(std::string("fs/").length());
    } else {
        req.action = pathPart;
    }

    return req;
}

std::string XplmBridge::getInjectionScript() {
    return R"(
(function() {
    if (window.skyscript && window.skyscript.xplm) return;

    window.skyscript = window.skyscript || {};
    window.skyscript.version = ')" VERSION R"(';
    window.skyscript.xplaneVersion = ')" + std::to_string(XPLANE_VERSION) + R"(';
    window.skyscript._callbacks = window.skyscript._callbacks || {};
    window.skyscript._nextId = window.skyscript._nextId || 1;
    window.skyscript._messageListeners = window.skyscript._messageListeners || {};

    window.skyscript._resolve = function(id, value) {
        var cb = window.skyscript._callbacks[id];
        if (cb) {
            cb.resolve(value);
            delete window.skyscript._callbacks[id];
        }
    };

    window.skyscript._reject = function(id, error) {
        var cb = window.skyscript._callbacks[id];
        if (cb) {
            cb.reject(new Error(error));
            delete window.skyscript._callbacks[id];
        }
    };

    function sendRequest(scheme, action, params) {
        return new Promise(function(resolve, reject) {
            var id = window.skyscript._nextId++;
            window.skyscript._callbacks[id] = { resolve: resolve, reject: reject };

            var query = 'callbackId=' + id;
            for (var key in params) {
                if (params.hasOwnProperty(key) && params[key] !== undefined) {
                    query += '&' + key + '=' + encodeURIComponent(params[key]);
                }
            }

            var iframe = document.createElement('iframe');
            iframe.style.display = 'none';
            iframe.src = 'skyscript://' + scheme + '/' + action + '?' + query;
            document.body.appendChild(iframe);
            setTimeout(function() {
                document.body.removeChild(iframe);
            }, 100);
        });
    }

    window.skyscript.xplm = {
        getDataref: function(ref) {
            return sendRequest('xplm', 'getDataref', { ref: ref });
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
            return sendRequest('xplm', 'setDataref', { ref: ref, value: String(value), valueType: type });
        },
        executeCommand: function(command) {
            return sendRequest('xplm', 'executeCommand', { ref: command });
        }
    };

    window.skyscript.postMessage = function(channel, payload) {
        var payloadStr = (payload === undefined) ? 'null' : JSON.stringify(payload);
        return sendRequest('message', 'postMessage', { channel: channel, payload: payloadStr });
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
            return sendRequest('notification', 'showNotification', { payload: JSON.stringify(config) });
        },
        dismiss: function() {
            return sendRequest('notification', 'dismissNotification', {});
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

    window.skyscript.fs = {
        readFile: function(path) {
            return sendRequest('fs', 'fsReadFile', { ref: path });
        },
        writeFile: function(path, content) {
            return sendRequest('fs', 'fsWriteFile', { ref: path, value: content });
        },
        listDir: function(path) {
            return sendRequest('fs', 'fsListDir', { ref: path });
        },
        exists: function(path) {
            return sendRequest('fs', 'fsExists', { ref: path });
        }
    };
})();
)";
}
