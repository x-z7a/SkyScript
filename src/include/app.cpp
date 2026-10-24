#include "app.h"

#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "browser.h"
#include "button.h"
#include "config.h"
#include "dataref.h"
#include "notification.h"
#include "path.h"

App* App::current = nullptr;

App::App(const std::string& aName, const std::string& anId, AppType aType, const AppConfiguration& aConfig) {
    name = aName;
    id = anId;
    type = aType;
    isDefault = false;
    config = aConfig;
    notification = nullptr;
    notificationOverlay = false;
    window = nullptr;
    overlayWindow = nullptr;
    visible = false;
    browser = nullptr;
    brightness = 1.0f;
    unsigned short initWidth = config.width > 0 ? config.width : defaultWindowWidth;
    unsigned short initHeight = config.height > 0 ? config.height : defaultWindowHeight;
    viewport = {0, 0, initWidth, initHeight};
}

App::~App() {
    deinitialize();
}

void App::initialize() {
    App::current = this;

    if (!browser) {
        browser = new Browser(this);
    }

    browser->initialize();

    std::string datarefPrefix = "skyscript/" + id;

    Dataref::getInstance()->createDataref<bool>((datarefPrefix + "/visible").c_str(), &visible);
    Dataref::getInstance()->createCommand((datarefPrefix + "/toggle").c_str(), ("Show or hide " + name).c_str(), [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (visible) {
            hideBrowser();
        }
        else {
            showBrowser();
        }
    });

    App::current = nullptr;
}

void App::deinitialize() {
    App::current = this;

    std::string datarefPrefix = "skyscript/" + id;
    Dataref::getInstance()->unbind((datarefPrefix + "/visible").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/toggle").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/url").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/refresh").c_str());

    clearNotification();

    if (browser) {
        browser->visibilityWillChange(false);
        browser->destroy();
        delete browser;
        browser = nullptr;
    }

    if (overlayWindow) {
        XPLMDestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }

    if (window) {
        XPLMDestroyWindow(window);
        window = nullptr;
    }

    tasks.clear();
    buttons.clear();
    notificationOverlay = false;
    visible = false;
    brightness = 1.0f;

    App::current = nullptr;
}

void App::update() {
    App::current = this;

    if (!browser) {
        App::current = nullptr;
        return;
    }

    brightness = 1.0f;

    // X-Plane's title bar has a close button, and closing the window that way
    // never goes through hideBrowser.
    if (visible && window && !XPLMGetWindowIsVisible(window)) {
        browser->visibilityWillChange(false);
        visible = false;
    }

    if (notificationOverlay) {
        syncOverlayGeometry();
    }

    browser->update();

    if (notification) {
        notification->update();
        if (notification->isFinished()) {
            clearNotification();
        }
    }

    tasks.erase(
        std::remove_if(tasks.begin(), tasks.end(), [&](const DelayedTask& task) {
            if (XPLMGetElapsedTime() > task.executeAfterElapsedSeconds) {
                task.func();
                return true;
            }

            return false;
        }),
        tasks.end()
    );

    App::current = nullptr;
}

// The overlay window's draw callback. The browser window draws itself, so all
// that is ever drawn here is the notification.
void App::draw() {
    if (!notificationOverlay || !notification || !overlayWindow) {
        return;
    }

    App::current = this;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(overlayWindow, &left, &top, &right, &bottom);
    setViewport(left, bottom, static_cast<unsigned short>((std::max)(1, right - left)), static_cast<unsigned short>((std::max)(1, top - bottom)));

    notification->draw();

    App::current = nullptr;
}

void App::syncOverlayGeometry() {
    if (!window || !overlayWindow) {
        return;
    }

    // The browser window keeps its last geometry while hidden, so a toast
    // raised with the app closed still lands where the app would be.
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    int overlayLeft, overlayTop, overlayRight, overlayBottom;
    XPLMGetWindowGeometry(overlayWindow, &overlayLeft, &overlayTop, &overlayRight, &overlayBottom);
    if (left != overlayLeft || top != overlayTop || right != overlayRight || bottom != overlayBottom) {
        XPLMSetWindowGeometry(overlayWindow, left, top, right, bottom);
    }
}

void App::applyWindowMode() {
    if (!window) {
        return;
    }

    bool isVrEnabled = Dataref::getInstance()->get<bool>("sim/graphics/VR/enabled");
    XPLMSetWindowPositioningMode(window, isVrEnabled ? xplm_WindowVR : xplm_WindowPositionFree, -1);

    // The overlay tracks the browser window's 2-D geometry, which has no
    // meaning for a VR window, so in VR it stands down.
    if (isVrEnabled && notificationOverlay) {
        clearNotification();
    }

    if (visible) {
        XPLMBringWindowToFront(window);
    }
}

void App::registerButton(Button *button) {
    buttons.push_back(button);
}

void App::unregisterButton(Button *button) {
    auto it = std::find(buttons.begin(), buttons.end(), button);
    if (it != buttons.end()) {
        buttons.erase(it);
    }
}

void App::showBrowser(std::string url) {
    if (!browser || !window) {
        return;
    }

    if (!url.empty()) {
        browser->loadUrl(url);
    }

    if (!visible) {
        browser->visibilityWillChange(true);
        visible = true;
        XPLMSetWindowIsVisible(window, 1);
        applyWindowMode();
    }

    XPLMBringWindowToFront(window);
    // Keep a toast that is already up on top of the window just raised.
    if (notificationOverlay && overlayWindow) {
        XPLMBringWindowToFront(overlayWindow);
    }
}

void App::hideBrowser() {
    if (!visible) {
        return;
    }

    browser->visibilityWillChange(false);
    visible = false;

    if (window) {
        XPLMSetWindowIsVisible(window, 0);
    }
}

void App::showNotification(Notification *aNotification) {
    if (!aNotification) {
        clearNotification();
        return;
    }

    if (notification && notification != aNotification) {
        clearNotification();
    }

    notification = aNotification;

    // A notification is worth showing whether or not the app is open -- that is
    // the whole point of a notification.
    setNotificationOverlay(true);
    if (!notificationOverlay) {
        // Nowhere to draw it (VR). Better gone than drawn on the next 2-D frame
        // long after the moment has passed.
        clearNotification();
    }
}

void App::showNotification(const NotificationOptions& options) {
    if (!canRaiseNotificationOverlay()) {
        debug("[%s] Notification (not shown in VR): %s %s\n", name.c_str(), options.title.c_str(), options.body.c_str());
        return;
    }

    App* previous = App::current;
    App::current = this;

    // The overlay passes every click through to whatever is below it, so
    // nothing on the toast can be clicked. Drawing a close button there would
    // offer an affordance that does nothing, and a notification that waits to
    // be dismissed would then never leave.
    NotificationOptions resolved = options;
    resolved.dismissible = false;
    if (resolved.timeoutSeconds <= 0.0f) {
        resolved.timeoutSeconds = Notification::defaultOptions().timeoutSeconds;
    }

    // The toast lays itself out against the viewport, so it needs the overlay's
    // geometry before it is built.
    syncOverlayGeometry();
    if (overlayWindow) {
        int left, top, right, bottom;
        XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);
        setViewport(left, bottom, static_cast<unsigned short>((std::max)(1, right - left)), static_cast<unsigned short>((std::max)(1, top - bottom)));
    }

    showNotification(new Notification(resolved));
    App::current = previous;
}

void App::dismissNotification() {
    if (notification) {
        notification->dismiss();
    }
}

void App::clearNotification() {
    if (!notification) {
        return;
    }

    App* previous = App::current;
    App::current = this;
    Notification *oldNotification = notification;
    notification = nullptr;
    delete oldNotification;
    App::current = previous;

    setNotificationOverlay(false);
}

void App::setNotificationOverlay(bool active) {
    if (notificationOverlay == active) {
        return;
    }

    if (active && !canRaiseNotificationOverlay()) {
        return;
    }

    notificationOverlay = active;

    if (!overlayWindow) {
        return;
    }

    if (active) {
        syncOverlayGeometry();
        XPLMSetWindowIsVisible(overlayWindow, 1);
        XPLMBringWindowToFront(overlayWindow);
    }
    else {
        XPLMSetWindowIsVisible(overlayWindow, 0);
    }
}

bool App::hasNotificationOverlay() const {
    return notificationOverlay;
}

bool App::canRaiseNotificationOverlay() const {
    if (!window || !overlayWindow) {
        return false;
    }

    return Dataref::getInstance()->getCached<int>("sim/graphics/VR/enabled") == 0;
}

NotificationOptions App::defaultNotificationOptions() const {
    NotificationOptions options = Notification::defaultOptions();
    options.corner = config.notification_corner;
    options.timeoutSeconds = config.notification_timeout;
    options.opacity = config.notification_opacity;
    options.slideSeconds = config.notification_slide_seconds;
    options.playSound = config.notification_sound;
    return options;
}

void App::executeDelayed(CallbackFunc func, float delaySeconds) {
    tasks.push_back({
        func,
        XPLMGetElapsedTime() + delaySeconds
    });
}

void App::onMessage(const std::string& channel, std::function<std::pair<std::string, std::string>(const std::string&)> handler) {
    if (browser) {
        browser->onMessage(channel, handler);
    }
}

void App::postMessageToJS(const std::string& channel, const std::string& payload) {
    if (browser) {
        browser->postMessage(channel, payload);
    }
}

void App::setViewport(int x, int y, unsigned short width, unsigned short height) {
    viewport = {
        x,
        y,
        (std::max<unsigned short>)(1, width),
        (std::max<unsigned short>)(1, height)
    };
}

void App::registerWindow() {
    if (overlayWindow) {
        XPLMDestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
    if (window) {
        XPLMDestroyWindow(window);
        window = nullptr;
    }

    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);

    float screenWidth = fabs(winLeft - winRight);
    float screenHeight = fabs(winTop - winBot);
    int width = config.width > 0 ? config.width : defaultWindowWidth;
    int height = config.height > 0 ? config.height : defaultWindowHeight;

    // Zero-initialised: from SDK 4.4.0 a NULL callback means "not interested",
    // and a browser window wants none of the input callbacks -- X-Plane routes
    // mouse and keyboard to the page itself.
    XPLMCreateWindow_t params = {};
    params.structSize = sizeof(params);
    params.left = (int)(winLeft + (screenWidth - width) / 2);
    params.right = params.left + width;
    params.top = (int)(winTop - (screenHeight - height) / 2);
    params.bottom = params.top - height;
    params.visible = 0;
    params.refcon = this;
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    params.contentType = xplm_WindowContentTypeBrowser;
    params.browserLoadFinishedFunc = [](XPLMWindowID inWindow, const char *inURL, void *inRefcon) {
        App *app = static_cast<App*>(inRefcon);
        App *previous = App::current;
        App::current = app;
        if (app->browser) {
            app->browser->onLoadFinished(inURL ? inURL : "");
        }
        App::current = previous;
    };
    params.browserLoadErrorFunc = [](XPLMWindowID inWindow, const char *inURL, const char *inError, void *inRefcon) {
        App *app = static_cast<App*>(inRefcon);
        App *previous = App::current;
        App::current = app;
        if (app->browser) {
            app->browser->onLoadError(inURL ? inURL : "", inError ? inError : "");
        }
        App::current = previous;
    };

    window = XPLMCreateWindowEx(&params);
    if (!window) {
        debug("Could not create the browser window for app '%s'. X-Plane 12.4.4 or newer is required.\n", id.c_str());
        return;
    }

    XPLMSetWindowTitle(window, name.c_str());
    XPLMSetWindowResizingLimits(
        window,
        (std::max)(static_cast<int>(config.minimum_width), minimumContentWidth),
        minimumContentHeight,
        4096,
        4096);

    browser->attach(window);
    registerOverlayWindow();
    applyWindowMode();
    XPLMSetWindowIsVisible(window, 0);
}

void App::registerOverlayWindow() {
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    // No decoration and no click handlers, so every click falls through to the
    // browser window or the cockpit below it. The growl layer keeps it above
    // floating windows, the browser window included.
    XPLMCreateWindow_t params = {};
    params.structSize = sizeof(params);
    params.left = left;
    params.top = top;
    params.right = right;
    params.bottom = bottom;
    params.visible = 0;
    params.refcon = this;
    params.drawWindowFunc = [](XPLMWindowID inWindowID, void *inRefcon) {
        static_cast<App*>(inRefcon)->draw();
    };
    params.layer = xplm_WindowLayerGrowlNotifications;
    params.decorateAsFloatingWindow = xplm_WindowDecorationNone;
    params.contentType = xplm_WindowContentTypeOpenGL;

    overlayWindow = XPLMCreateWindowEx(&params);
    if (!overlayWindow) {
        debug("Could not create the notification overlay for app '%s'.\n", id.c_str());
        return;
    }

    XPLMSetWindowIsVisible(overlayWindow, 0);
}

AppConfiguration App::defaultConfig() {
    AppConfiguration config;
    config.homepage = "https://www.google.com";
    config.audio_muted = false;
    config.minimum_width = 0;
    config.scroll_speed = 5;
    config.forced_language = "";
    config.user_agent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/117.2.5.0 Safari/537.36";
    config.hide_addressbar = false;
    config.framerate = 25;
    config.width = 0;
    config.height = 0;
    config.console_logging = true;
    NotificationOptions notificationDefaults = Notification::defaultOptions();
    config.notification_corner = notificationDefaults.corner;
    config.notification_timeout = notificationDefaults.timeoutSeconds;
    config.notification_opacity = notificationDefaults.opacity;
    config.notification_slide_seconds = notificationDefaults.slideSeconds;
    config.notification_sound = notificationDefaults.playSound;
    config.window_titleless = true;
    config.window_opacity = 0.96f;
#if DEBUG
    config.debug_value_1 = 0.0f;
    config.debug_value_2 = 0.0f;
    config.debug_value_3 = 0.0f;
#endif
    return config;
}

AppConfiguration App::parseManifest(const std::string& manifestPath, const AppConfiguration& defaults) {
    AppConfiguration config = defaults;

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim whitespace
        auto trimStart = key.find_first_not_of(" \t");
        auto trimEnd = key.find_last_not_of(" \t");
        if (trimStart != std::string::npos) {
            key = key.substr(trimStart, trimEnd - trimStart + 1);
        }

        trimStart = value.find_first_not_of(" \t");
        trimEnd = value.find_last_not_of(" \t\r\n");
        if (trimStart != std::string::npos) {
            value = value.substr(trimStart, trimEnd - trimStart + 1);
        }
        else {
            value = "";
        }

        // Strip inline comments
        size_t commentPos = value.find(" #");
        if (commentPos != std::string::npos) {
            value = value.substr(0, commentPos);
            trimEnd = value.find_last_not_of(" \t");
            if (trimEnd != std::string::npos) {
                value = value.substr(0, trimEnd + 1);
            }
        }

        if (key == "name") {
            // name is handled externally, not in AppConfiguration
        }
        else if (key == "url" || key == "homepage") {
            if (!value.empty()) {
                config.homepage = value;
            }
        }
        else if (key == "framerate") {
            if (!value.empty()) {
                config.framerate = std::stoi(value);
            }
        }
        else if (key == "audio_muted") {
            config.audio_muted = (value == "true" || value == "1");
        }
        else if (key == "minimum_width") {
            if (!value.empty()) {
                config.minimum_width = std::stoi(value);
            }
        }
        else if (key == "hide_addressbar") {
            config.hide_addressbar = (value == "true" || value == "1");
        }
        else if (key == "scroll_speed") {
            if (!value.empty()) {
                config.scroll_speed = std::stoi(value);
            }
        }
        else if (key == "forced_language") {
            config.forced_language = value;
        }
        else if (key == "user_agent") {
            if (!value.empty()) {
                config.user_agent = value;
            }
        }
        else if (key == "width") {
            if (!value.empty()) {
                config.width = std::stoi(value);
            }
        }
        else if (key == "height") {
            if (!value.empty()) {
                config.height = std::stoi(value);
            }
        }
        else if (key == "console_logging") {
            config.console_logging = (value != "false" && value != "0");
        }
        else if (key == "notification_corner" || key == "notification_location") {
            config.notification_corner = NotificationCornerFromString(value, config.notification_corner);
        }
        else if (key == "notification_timeout" || key == "notification_timeout_seconds") {
            if (!value.empty()) {
                config.notification_timeout = std::stof(value);
            }
        }
        else if (key == "notification_opacity") {
            if (!value.empty()) {
                config.notification_opacity = std::stof(value);
            }
        }
        else if (key == "notification_slide_seconds") {
            if (!value.empty()) {
                config.notification_slide_seconds = std::stof(value);
            }
        }
        else if (key == "notification_sound") {
            config.notification_sound = (value != "false" && value != "0");
        }
        else if (key == "window_titleless") {
            config.window_titleless = (value != "false" && value != "0");
        }
        else if (key == "window_opacity") {
            if (!value.empty()) {
                config.window_opacity = std::stof(value);
            }
        }
    }

    return config;
}
