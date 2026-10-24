#ifndef BROWSER_H
#define BROWSER_H

#include <string>
#include <XPLMDisplay.h>
#include <XPLMDefs.h>

#include "xplm_bridge.h"

class App;

// The page inside an app window.
//
// X-Plane renders the page, routes input to it and owns the Chromium instance
// (xplm_WindowContentTypeBrowser, SDK 4.4.0). What is left for us is choosing
// what it loads, injecting the window.skyscript API once a page has loaded, and
// answering that API through a single xplane.skyscript() browser function.
class Browser {
private:
    App *app;
    XPLMWindowID window;
    float lastGpsUpdateTime;
    XplmBridge *xplmBridge;
    void injectScript(const std::string& script);
    void injectPageScripts();
    void updateGPSLocation();
    static const char* handleBrowserCall(XPLMWindowID inWindowID, const char* inJSON, void* inRefcon);

public:
    explicit Browser(App *app);

    std::string currentUrl;

    void initialize();
    // Bind to the browser window once App has created it: registers the bridge
    // function and loads the current URL. X-Plane queues the load until its
    // webview is ready, so this is safe straight after XPLMCreateWindowEx.
    void attach(XPLMWindowID window);
    void destroy();
    void visibilityWillChange(bool becomesVisible);
    void update();
    void loadUrl(const std::string& url);
    void refresh(bool ignoreCache);
    void onLoadFinished(const std::string& url);
    void onLoadError(const std::string& url, const std::string& error);

    // Message passing
    void onMessage(const std::string& channel, MessageHandler handler);
    void postMessage(const std::string& channel, const std::string& payload);

    // A file:// URL for a local path, valid on every platform. Spaces and other
    // reserved characters are percent-encoded, and Windows drive paths get the
    // third slash ("file:///C:/...") a bare concatenation would miss.
    static std::string fileUrlForPath(const std::string& path);
};

#endif
