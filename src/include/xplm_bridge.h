#ifndef XPLM_BRIDGE_H
#define XPLM_BRIDGE_H

#include <mutex>
#include <string>
#include <vector>
#include <include/cef_browser.h>

struct XplmRequest {
    std::string action;      // "getDataref", "setDataref", "executeCommand"
    std::string ref;         // dataref path or command path
    std::string value;       // for setDataref: stringified value
    std::string valueType;   // "int", "float", "double", "string"
    int callbackId;
    CefRefPtr<CefBrowser> browser;
};

class XplmBridge {
private:
    std::mutex mutex;
    std::vector<XplmRequest> pendingRequests;

    void handleGetDataref(const XplmRequest& req);
    void handleSetDataref(const XplmRequest& req);
    void handleExecuteCommand(const XplmRequest& req);
    void respond(const XplmRequest& req, const std::string& jsValue, bool isError = false);

public:
    void enqueueRequest(const XplmRequest& req);
    void processPendingRequests();

    static std::string getInjectionScript();
    static XplmRequest parseRequestUrl(const std::string& url, CefRefPtr<CefBrowser> browser);
};

#endif
