#include "xplm_bridge.h"
#include "dataref.h"
#include "config.h"

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
        } else {
            respond(req, "\"Unknown action: " + req.action + "\"", true);
        }
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

    // URL format: skyscript://xplm/<action>?ref=...&callbackId=...&value=...&valueType=...
    // Parse action from path
    std::string prefix = "skyscript://xplm/";
    std::string remainder = url.substr(prefix.length());

    std::string pathPart = remainder;
    std::string queryPart;
    auto qpos = remainder.find('?');
    if (qpos != std::string::npos) {
        pathPart = remainder.substr(0, qpos);
        queryPart = remainder.substr(qpos + 1);
    }

    req.action = pathPart;

    // Parse query parameters
    if (!queryPart.empty()) {
        CefString query(queryPart);
        std::multimap<CefString, CefString> params;

        // CefParseURLQuery exists -- use manual parsing for compatibility
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
        }
    }

    return req;
}

std::string XplmBridge::getInjectionScript() {
    return R"(
(function() {
    if (window.skyscript && window.skyscript.xplm) return;

    window.skyscript = window.skyscript || {};
    window.skyscript._callbacks = window.skyscript._callbacks || {};
    window.skyscript._nextId = window.skyscript._nextId || 1;

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

    function sendRequest(action, params) {
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
            iframe.src = 'skyscript://xplm/' + action + '?' + query;
            document.body.appendChild(iframe);
            setTimeout(function() {
                document.body.removeChild(iframe);
            }, 100);
        });
    }

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
})();
)";
}
