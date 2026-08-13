#ifndef BROWSER_H
#define BROWSER_H

#include <filesystem>
#include <XPLMDisplay.h>
#include <XPLMDefs.h>

#include "button.h"

#include <include/cef_app.h>
#include <include/cef_request_context.h>
#include "browser_handler.h"
#include "xplm_bridge.h"

class App;

class Browser {
private:
    int textureId;
    float offsetStart;
    float offsetEnd;
    float lastGpsUpdateTime;
    Button *backButton;
    XplmBridge *xplmBridge;
    CefRefPtr<BrowserHandler> handler;
    CefRefPtr<CefRequestContext> requestContext;
    std::string localAppHost;
    std::filesystem::path localAppRoot;
    void allocateTexture();
    bool createBrowser();
    void updateGPSLocation();
    CefMouseEvent getMouseEvent(float normalizedX, float normalizedY);
    void registerLocalAppRoot();
    std::string prepareUrlForLoad(const std::string& url);
    
public:
    Browser();
    
    std::string currentUrl;
    
    void initialize();
    void destroy();
    void visibilityWillChange(bool becomesVisible);
    void update();
    void draw();
    void resize();
    void loadUrl(std::string url);
    bool hasInputFocus();
    void setFocus(bool focus);
    void mouseMove(float normalizedX, float normalizedY);
    bool click(XPLMMouseStatus status, float normalizedX, float normalizedY);
    void scroll(float normalizedX, float normalizedY, int clicks, bool horizontal);
    void key(unsigned char key, unsigned char virtualKey, XPLMKeyFlags flags = 0);
    bool goBack();
    void showDevTools();
    CursorType cursor();

    // Message passing
    void onMessage(const std::string& channel, MessageHandler handler);
    void postMessage(const std::string& channel, const std::string& payload);
};

#endif
