#ifndef XPLM_BRIDGE_H
#define XPLM_BRIDGE_H

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <include/cef_browser.h>

struct XplmRequest {
    std::string action;      // "getDataref", "setDataref", "executeCommand", "postMessage", "showNotification", "dismissNotification", "fsReadFile", "fsWriteFile", "fsListDir", "fsExists"
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

// A plugin-to-JS message waiting to be delivered on the main thread.
struct PendingPost {
    std::string channel;
    std::string payload;
};

class XplmBridge {
private:
    // Bound on the queue a plugin can fill from its own thread. Generous enough
    // that ordinary bursts survive a slow frame, small enough that a plugin
    // posting in a loop with a hidden window cannot grow it without limit.
    static constexpr std::size_t kMaxPendingPosts = 1024;

    std::mutex mutex;
    std::vector<XplmRequest> pendingRequests;

    std::mutex handlersMutex;
    std::unordered_map<std::string, MessageHandler> messageHandlers;

    std::mutex postsMutex;
    std::vector<PendingPost> pendingPosts;

    CefRefPtr<CefBrowser> lastBrowser;
    // Readable from any thread, so postMessage can tell whether there is a
    // browser to post to without touching the refcounted pointer itself.
    std::atomic<bool> browserReady{false};

    void handleGetDataref(const XplmRequest& req);
    void handleSetDataref(const XplmRequest& req);
    void handleExecuteCommand(const XplmRequest& req);
    void handlePostMessage(const XplmRequest& req);
    void handleShowNotification(const XplmRequest& req);
    void handleDismissNotification(const XplmRequest& req);
    void handleFsReadFile(const XplmRequest& req);
    void handleFsWriteFile(const XplmRequest& req);
    void handleFsListDir(const XplmRequest& req);
    void handleFsExists(const XplmRequest& req);
    void deliverPost(const PendingPost& post);
    void respond(const XplmRequest& req, const std::string& jsValue, bool isError = false);

    static std::string escapeJsString(const std::string& str);
    bool isPathAllowed(const std::string& path) const;

public:
    void enqueueRequest(const XplmRequest& req);
    void processPendingRequests();

    // Register a handler for messages from JS on a given channel.
    void onMessage(const std::string& channel, MessageHandler handler);

    // Push a message from the plugin to JS on a given channel.
    // Safe to call from any thread: the message is queued and delivered on the
    // next processPendingRequests, which runs on the thread that pumps CEF.
    // The payload must be a JSON document; it is parsed, not evaluated.
    void postMessage(const std::string& channel, const std::string& payload);

    // Set the browser instance reference for posting messages to JS.
    void setBrowser(CefRefPtr<CefBrowser> browser);

    static std::string getInjectionScript();
    static XplmRequest parseRequestUrl(const std::string& url, CefRefPtr<CefBrowser> browser);
};

#endif
