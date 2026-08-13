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
    bool setViewport(int x, int y, unsigned short width, unsigned short height);
    void drawWindowBackground();
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
    static constexpr float browserTopRatio = 1.0f;
    static constexpr float toolbarY = 0.985f;

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
