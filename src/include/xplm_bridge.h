#ifndef XPLM_BRIDGE_H
#define XPLM_BRIDGE_H

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <include/cef_browser.h>

struct XplmRequest {
    std::string action;      // "getDataref", "setDataref", "executeCommand", "postMessage", "fsReadFile", "fsWriteFile", "fsListDir", "fsExists"
    std::string ref;         // dataref path or command path
    std::string value;       // for setDataref: stringified value; for fs: content/path
    std::string valueType;   // "int", "float", "double", "string"
    std::string channel;     // for message passing: channel name
    std::string payload;     // for message passing: JSON payload
    int callbackId;
    CefRefPtr<CefBrowser> browser;
};

// Callback type for handling messages from JS.
// Receives the JSON payload string and returns (response, error).
// If error is non-empty, the JS Promise is rejected.
using MessageHandler = std::function<std::pair<std::string, std::string>(const std::string& payload)>;

class XplmBridge {
private:
    std::mutex mutex;
    std::vector<XplmRequest> pendingRequests;

    std::mutex handlersMutex;
    std::unordered_map<std::string, MessageHandler> messageHandlers;

    CefRefPtr<CefBrowser> lastBrowser;

    void handleGetDataref(const XplmRequest& req);
    void handleSetDataref(const XplmRequest& req);
    void handleExecuteCommand(const XplmRequest& req);
    void handlePostMessage(const XplmRequest& req);
    void handleFsReadFile(const XplmRequest& req);
    void handleFsWriteFile(const XplmRequest& req);
    void handleFsListDir(const XplmRequest& req);
    void handleFsExists(const XplmRequest& req);
    void respond(const XplmRequest& req, const std::string& jsValue, bool isError = false);

    static std::string escapeJsString(const std::string& str);
    bool isPathAllowed(const std::string& path) const;

public:
    void enqueueRequest(const XplmRequest& req);
    void processPendingRequests();

    // Register a handler for messages from JS on a given channel.
    void onMessage(const std::string& channel, MessageHandler handler);

    // Push a message from the plugin to JS on a given channel.
    void postMessage(const std::string& channel, const std::string& payload);

    // Set the browser instance reference for posting messages to JS.
    void setBrowser(CefRefPtr<CefBrowser> browser);

    static std::string getInjectionScript();
    static XplmRequest parseRequestUrl(const std::string& url, CefRefPtr<CefBrowser> browser);
};

#endif
