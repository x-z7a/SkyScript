#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <functional>
#include <XPLMDisplay.h>
#include "notification.h"

class Browser;

#include "button.h"

enum class AppType { Folder };

// The rectangle the notification overlay draws into, in boxels.
struct WindowViewport {
    int x;
    int y;
    unsigned short width;
    unsigned short height;
};

struct AppConfiguration {
    std::string homepage;
    // Kept so the C API's config struct still maps one to one, but X-Plane owns
    // the browser now and offers no way to apply it.
    bool audio_muted;
    // Raises the window's minimum width.
    unsigned short minimum_width;
    // No longer used: X-Plane scrolls the page itself.
    unsigned char scroll_speed;
    // No longer used: X-Plane sets the browser language.
    std::string forced_language;
    // Applied to navigator.userAgent only; X-Plane sends its own header.
    std::string user_agent;
    bool hide_addressbar;
    // No longer used: X-Plane paces the browser.
    unsigned char framerate;
    unsigned short width;
    unsigned short height;
    bool console_logging;
    NotificationCorner notification_corner;
    float notification_timeout;
    float notification_opacity;
    float notification_slide_seconds;
    bool notification_sound;
    // SkyScript's own title bar and resize grip instead of X-Plane's title bar.
    // A browser window draws itself, so the chrome lives in two small windows
    // of our own glued above and below it.
    bool window_titleless;
    // Page opacity for titleless windows, applied as CSS on the page.
    float window_opacity;
#if DEBUG
    float debug_value_1;
    float debug_value_2;
    float debug_value_3;
#endif
};

typedef std::function<void()> CallbackFunc;

struct DelayedTask {
    CallbackFunc func;
    float executeAfterElapsedSeconds;
};

class App {
private:
    std::vector<DelayedTask> tasks;
    std::vector<Button *> buttons;
    Notification *notification;
    // True while the overlay window is up carrying a notification.
    bool notificationOverlay;
    // Window drag from the title bar and resize from the footer grip. Both move
    // the browser window; the chrome windows follow it.
    bool windowDragActive;
    bool windowResizeActive;
    int dragStartX;
    int dragStartY;
    int dragLeft;
    int dragTop;
    int dragRight;
    int dragBottom;
    bool closeButtonPressed;
    void setViewport(int x, int y, unsigned short width, unsigned short height);
    void registerOverlayWindow();
    // Keep the overlay on top of the browser window, so a toast sits in the
    // app's corner whether or not the app is open.
    void syncOverlayGeometry();
    // Raise or drop the notification overlay window.
    void setNotificationOverlay(bool active);
    // Whether a notification raised right now would be visible.
    bool canRaiseNotificationOverlay() const;
    void registerChromeWindows();
    // Glue the title bar and footer windows to the browser window's edges.
    // Called every frame they draw, so they follow X-Plane's own edge resizing.
    void syncChromeGeometry();
    // Chrome is shown with the browser, and never in VR, where X-Plane frames
    // the window itself.
    void updateChromeVisibility();
    bool usesChrome() const;
    void drawTitleBar();
    void drawFooter();
    int handleTitleBarClick(int x, int y, XPLMMouseStatus status);
    int handleFooterClick(int x, int y, XPLMMouseStatus status);
    XPLMCursorStatus titleBarCursor(int x, int y);
    XPLMCursorStatus footerCursor(int x, int y);
    bool isCloseButtonPoint(int x, int y) const;
    bool isResizeGripPoint(int x, int y) const;
    void setViewportFromWindow(XPLMWindowID window);

public:
    static App* current;

    static constexpr unsigned short defaultWindowWidth = 1024;
    static constexpr unsigned short defaultWindowHeight = 768;

    // Chrome for titleless windows, in boxels: a title bar above the page and
    // a footer carrying the resize grip below it. They sit outside the page,
    // so a manifest's width/height stay the size of the page.
    static constexpr int titleBarHeight = 28;
    static constexpr int resizeFooterHeight = 16;
    // Side of the square hit area for the resize grip, at the footer's right end.
    static constexpr int resizeGripSize = 20;
    // Side of the square hit area for the close button, at the title bar's left end.
    static constexpr int closeButtonSize = 28;

    // Smallest page the window may be shrunk to.
    static constexpr int minimumContentWidth = 640;
    static constexpr int minimumContentHeight = 480;

    std::string name;
    std::string id;
    AppType type;
    bool isDefault;

    // The browser window. X-Plane renders the page into it and routes input to
    // it; we never draw into it.
    XPLMWindowID window;
    // A transparent, click-through OpenGL window over the browser window. A
    // browser window ignores drawing callbacks, so notifications need a window
    // of their own to be drawn in.
    XPLMWindowID overlayWindow;
    // The chrome of a titleless window: OpenGL windows above and below the
    // browser window. nullptr for a window with X-Plane's own title bar.
    XPLMWindowID titleWindow;
    XPLMWindowID footerWindow;
    float brightness;
    WindowViewport viewport;
    AppConfiguration config;
    bool visible;
    Browser *browser;

    App(const std::string& name, const std::string& id, AppType type, const AppConfiguration& config);
    ~App();

    void initialize();
    void deinitialize();
    void registerWindow();

    void update();
    void draw();
    void applyWindowMode();

    void registerButton(Button *button);
    void unregisterButton(Button *button);

    void showBrowser(std::string url = "");
    void hideBrowser();
    void showNotification(Notification *notification);
    void showNotification(const NotificationOptions& options);
    void dismissNotification();
    void clearNotification();
    // True while a notification is on screen. The overlay passes input straight
    // through, so the toast is presentation only.
    bool hasNotificationOverlay() const;
    NotificationOptions defaultNotificationOptions() const;
    void executeDelayed(CallbackFunc func, float delaySeconds);

    // Register a handler for messages from JS on a given channel.
    void onMessage(const std::string& channel, std::function<std::pair<std::string, std::string>(const std::string&)> handler);

    // Push a message from the plugin to JS on a given channel.
    void postMessageToJS(const std::string& channel, const std::string& payload);

    static AppConfiguration defaultConfig();
    static AppConfiguration parseManifest(const std::string& manifestPath, const AppConfiguration& defaults);
};

#endif
