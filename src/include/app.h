#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <functional>
#include <XPLMDisplay.h>
#include "cursor.h"
#include "notification.h"

class Browser;

#include "button.h"

enum class AppType { Folder };

struct WindowViewport {
    int x;
    int y;
    unsigned short width;
    unsigned short height;
    unsigned short textureWidth;
    unsigned short textureHeight;
    unsigned short browserWidth;
    unsigned short browserHeight;
    static constexpr unsigned char bytesPerPixel = 4;
};

struct AppConfiguration {
    std::string homepage;
    bool audio_muted;
    unsigned short minimum_width;
    unsigned char scroll_speed;
    std::string forced_language;
    std::string user_agent;
    bool hide_addressbar;
    unsigned char framerate;
    unsigned short width;
    unsigned short height;
    bool console_logging;
    NotificationCorner notification_corner;
    float notification_timeout;
    float notification_opacity;
    float notification_slide_seconds;
    bool notification_sound;
    bool window_titleless;
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
    // True while the X-Plane window is up solely to draw a notification, with
    // the browser hidden. See setNotificationOverlay.
    bool notificationOverlay;
    bool windowDragActive;
    int windowDragStartX;
    int windowDragStartY;
    int windowDragLeft;
    int windowDragTop;
    int windowDragRight;
    int windowDragBottom;
    bool windowResizeActive;
    int windowResizeStartX;
    int windowResizeStartY;
    int windowResizeLeft;
    int windowResizeTop;
    int windowResizeRight;
    int windowResizeBottom;
    bool setViewport(int x, int y, unsigned short width, unsigned short height);
    void drawWindowBackground();
    void drawTitleBar();
    void drawResizeFooter();
    bool isWindowDragRegion(float normalizedX, float normalizedY) const;
    // Raise or drop the notification-only window. Raising shows the X-Plane
    // window without showing the browser, so draw() paints the toast and
    // nothing else; dropping hides it again unless the browser is up.
    void setNotificationOverlay(bool active);
    // Whether a notification raised right now would carry its own window.
    bool canRaiseNotificationOverlay() const;

public:
    static App* current;

    static constexpr unsigned short defaultWindowWidth = 1024;
    static constexpr unsigned short defaultWindowHeight = 768;

    // Chrome we draw ourselves in titleless mode, in boxels: a title bar above
    // the page and a footer carrying the resize grip below it.
    //
    // The page is laid out inside what is left, and registerWindow adds these
    // back on to the size the manifest asked for. Before this, the page filled
    // the window and the drag strip was painted over the top of it, so an app's
    // own header sat underneath our chrome. Reserving the space instead is what
    // keeps a manifest's width/height meaning the size of the *page*.
    static constexpr float titleBarHeight = 28.0f;
    static constexpr float resizeFooterHeight = 16.0f;
    // Side of the square hit area for the resize grip, inset into the footer's
    // right end.
    static constexpr float resizeGripSize = 14.0f;

    // Smallest page the window may be shrunk to. The window's own floor is
    // this plus the chrome, so the limit means the same thing before and after
    // the chrome existed.
    static constexpr int minimumContentWidth = 640;
    static constexpr int minimumContentHeight = 480;

    std::string name;
    std::string id;
    AppType type;
    bool isDefault;

    XPLMWindowID window;
    float brightness;
    WindowViewport viewport;
    AppConfiguration config;
    bool visible;
    bool mouseDown;
    Browser *browser;
    CursorType activeCursor;

    App(const std::string& name, const std::string& id, AppType type, const AppConfiguration& config);
    ~App();

    void initialize();
    void deinitialize();
    void registerWindow();

    void update();
    void draw();
    bool syncWindowGeometry(bool resizeBrowser = true);
    bool normalizeWindowPoint(int x, int y, float *normalizedX, float *normalizedY);
    void applyWindowMode();

    bool updateButtons(float normalizedX, float normalizedY, ButtonState state);
    void registerButton(Button *button);
    void unregisterButton(Button *button);
    bool beginWindowDrag(float normalizedX, float normalizedY, int x, int y);
    bool dragWindow(int x, int y);
    void endWindowDrag();
    bool isDraggingWindow() const;

    // Boxels reserved above and below the page. Both are zero when X-Plane
    // draws the decoration: there the simulator owns the title bar and the
    // resizing, so there is nothing for us to keep clear.
    float titleBarBoxels() const;
    float footerBoxels() const;
    float chromeBoxels() const;
    // The page's extent within the window, normalized, bottom then top.
    float pageBottomRatio() const;
    float pageTopRatio() const;

    // The resize grip. X-Plane already resizes a self-decorated-resizable
    // window from its edges, but nothing on screen says so; this is the
    // affordance, and it drives the geometry itself so it behaves the same on
    // every platform.
    bool isResizeGripRegion(float normalizedX, float normalizedY) const;
    bool beginWindowResize(float normalizedX, float normalizedY, int x, int y);
    bool resizeWindow(int x, int y);
    void endWindowResize();
    bool isResizingWindow() const;

    void showBrowser(std::string url = "");
    void hideBrowser();
    void showNotification(Notification *notification);
    void showNotification(const NotificationOptions& options);
    void dismissNotification();
    void clearNotification();
    // True while a notification is being drawn over a hidden browser. Input is
    // passed straight through to the simulator in that state, so the toast is
    // presentation only.
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
