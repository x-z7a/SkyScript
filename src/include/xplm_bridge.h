#ifndef XPLM_BRIDGE_H
#define XPLM_BRIDGE_H

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <XPLMDisplay.h>

struct XplmRequest {
    std::string action;      // "getDataref", "setDataref", "executeCommand", "postMessage", "showNotification", "dismissNotification", "fsReadFile", "fsWriteFile", "fsListDir", "fsExists", "consoleLog"
    std::string ref;         // dataref path or command path
    std::string value;       // for setDataref: stringified value; for fs: content/path
    std::string valueType;   // "int", "float", "double", "string"
    std::string channel;     // for message passing: channel name
    std::string payload;     // for message passing: JSON payload
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

// The plugin side of window.skyscript.
//
// Requests arrive through X-Plane's browser-function bridge: the injected API
// calls xplane.skyscript({action, ...}), X-Plane hands us the argument as JSON
// on the main thread, and the JSON we hand back through XPLMReturnString
// resolves the page's Promise.
// That reply is always an envelope, {"ok":true,"value":...} or
// {"ok":false,"error":"..."}, which the injected API turns into resolve/reject.
class XplmBridge {
private:
    // Bound on the queue a plugin can fill from its own thread. Generous enough
    // that ordinary bursts survive a slow frame, small enough that a plugin
    // posting in a loop with a hidden window cannot grow it without limit.
    static constexpr std::size_t kMaxPendingPosts = 1024;

    std::mutex handlersMutex;
    std::unordered_map<std::string, MessageHandler> messageHandlers;

    std::mutex postsMutex;
    std::vector<PendingPost> pendingPosts;

    XPLMWindowID window = nullptr;
    // Readable from any thread, so postMessage can tell whether there is a
    // window to post to without touching the window itself.
    std::atomic<bool> windowReady{false};

    // Each returns the JSON text of the value the page's Promise resolves to,
    // or nothing for a call that resolves to undefined. Failures throw, and the
    // message becomes the rejection.
    std::optional<std::string> dispatch(const XplmRequest& req);
    std::optional<std::string> handlePostMessage(const XplmRequest& req);
    void deliverPost(const PendingPost& post);

public:
    // The page calls this as xplane.skyscript(request).
    static constexpr const char* functionName = "skyscript";

    // Answer one request from the page with its JSON envelope. The caller
    // hands it to X-Plane through XPLMReturnString.
    std::string handleRequest(const char* json);

    // Deliver queued plugin-to-JS messages. Main thread only.
    void processPendingPosts();

    // Register a handler for messages from JS on a given channel.
    void onMessage(const std::string& channel, MessageHandler handler);

    // Push a message from the plugin to JS on a given channel.
    // Safe to call from any thread: the message is queued and delivered on the
    // next processPendingPosts, which runs on the main thread from the flight
    // loop. The payload must be a JSON document; it is parsed, not evaluated.
    void postMessage(const std::string& channel, const std::string& payload);

    // The browser window to inject into, or nullptr once it is gone.
    void setWindow(XPLMWindowID window);

    static std::string getInjectionScript(bool consoleLogging);
};

#endif
