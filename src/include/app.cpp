#include "app.h"

#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>

#include "browser.h"
#include "button.h"
#include "config.h"
#include "dataref.h"
#include "drawing.h"
#include "notification.h"
#include "path.h"

namespace {

// OpenGL-drawn windows are created with the pre-4.4 struct, which ends just
// before contentType. Created with the 4.4 struct, X-Plane 12.4.4's legacy GL
// bridge drew their glBegin/glVertex geometry flipped top to bottom while text
// came out right; the older struct is the path SkyScript's GL drawing has
// always used. The old API wants every callback, so these fill the gaps.
constexpr int kLegacyWindowStructSize = static_cast<int>(offsetof(XPLMCreateWindow_t, contentType));

int ignoreClick(XPLMWindowID, int, int, XPLMMouseStatus, void*) { return 0; }
int consumeClick(XPLMWindowID, int, int, XPLMMouseStatus, void*) { return 1; }
void ignoreKey(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) {}
XPLMCursorStatus defaultCursor(XPLMWindowID, int, int, void*) { return xplm_CursorDefault; }
int ignoreWheel(XPLMWindowID, int, int, int, int, void*) { return 0; }
int consumeWheel(XPLMWindowID, int, int, int, int, void*) { return 1; }

// Once per window: what the GL bridge hands a draw callback, for diagnosing
// drawing that lands in the wrong place.
void logGlStateOnce(const char *label, XPLMWindowID window, bool *logged) {
    if (*logged) {
        return;
    }
    *logged = true;

    GLint glViewport[4] = {0, 0, 0, 0};
    GLfloat projection[16] = {0};
    GLfloat modelview[16] = {0};
    glGetIntegerv(GL_VIEWPORT, glViewport);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);

    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);
    debug("%s window l=%d t=%d r=%d b=%d; GL viewport %d %d %d %d; projection sx=%.5f sy=%.5f tx=%.3f ty=%.3f; modelview sy=%.3f ty=%.3f\n",
          label, left, top, right, bottom,
          glViewport[0], glViewport[1], glViewport[2], glViewport[3],
          projection[0], projection[5], projection[12], projection[13],
          modelview[5], modelview[13]);
}

} // namespace

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
    titleWindow = nullptr;
    footerWindow = nullptr;
    windowDragActive = false;
    windowResizeActive = false;
    dragStartX = 0;
    dragStartY = 0;
    dragLeft = 0;
    dragTop = 0;
    dragRight = 0;
    dragBottom = 0;
    closeButtonPressed = false;
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

    if (titleWindow) {
        XPLMDestroyWindow(titleWindow);
        titleWindow = nullptr;
    }

    if (footerWindow) {
        XPLMDestroyWindow(footerWindow);
        footerWindow = nullptr;
    }

    if (window) {
        XPLMDestroyWindow(window);
        window = nullptr;
    }

    windowDragActive = false;
    windowResizeActive = false;
    closeButtonPressed = false;
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

    updateChromeVisibility();

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

    updateChromeVisibility();

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
    updateChromeVisibility();
    if (titleWindow && footerWindow && usesChrome()) {
        XPLMBringWindowToFront(titleWindow);
        XPLMBringWindowToFront(footerWindow);
    }
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

    windowDragActive = false;
    windowResizeActive = false;
    closeButtonPressed = false;

    if (window) {
        XPLMSetWindowIsVisible(window, 0);
    }
    updateChromeVisibility();
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
    if (titleWindow) {
        XPLMDestroyWindow(titleWindow);
        titleWindow = nullptr;
    }
    if (footerWindow) {
        XPLMDestroyWindow(footerWindow);
        footerWindow = nullptr;
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
    // The manifest size is the page. Titleless chrome sits outside it, so the
    // whole frame is centred and the page placed below the title bar.
    int chromeTop = config.window_titleless ? titleBarHeight : 0;
    int chromeBottom = config.window_titleless ? resizeFooterHeight : 0;
    int frameHeight = height + chromeTop + chromeBottom;

    // Zero-initialised: from SDK 4.4.0 a NULL callback means "not interested",
    // and a browser window wants none of the input callbacks -- X-Plane routes
    // mouse and keyboard to the page itself.
    XPLMCreateWindow_t params = {};
    params.structSize = sizeof(params);
    params.left = (int)(winLeft + (screenWidth - width) / 2);
    params.right = params.left + width;
    params.top = (int)(winTop - (screenHeight - frameHeight) / 2) - chromeTop;
    params.bottom = params.top - height;
    params.visible = 0;
    params.refcon = this;
    params.layer = xplm_WindowLayerFloatingWindows;
    // Self-decorated-resizable gives a titleless window X-Plane's edge resizing
    // without its title bar; the chrome windows supply the rest.
    params.decorateAsFloatingWindow = config.window_titleless ? xplm_WindowDecorationSelfDecoratedResizable : xplm_WindowDecorationRoundRectangle;
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

    if (!config.window_titleless) {
        XPLMSetWindowTitle(window, name.c_str());
    }
    XPLMSetWindowResizingLimits(
        window,
        (std::max)(static_cast<int>(config.minimum_width), minimumContentWidth),
        minimumContentHeight,
        4096,
        4096);

    browser->attach(window);
    registerOverlayWindow();
    if (config.window_titleless) {
        registerChromeWindows();
    }
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
    params.structSize = kLegacyWindowStructSize;
    params.left = left;
    params.top = top;
    params.right = right;
    params.bottom = bottom;
    params.visible = 0;
    params.refcon = this;
    params.drawWindowFunc = [](XPLMWindowID inWindowID, void *inRefcon) {
        static_cast<App*>(inRefcon)->draw();
    };
    // Returning 0 passes every click and scroll on to whatever is below.
    params.handleMouseClickFunc = ignoreClick;
    params.handleRightClickFunc = ignoreClick;
    params.handleMouseWheelFunc = ignoreWheel;
    params.handleKeyFunc = ignoreKey;
    params.handleCursorFunc = defaultCursor;
    params.layer = xplm_WindowLayerGrowlNotifications;
    params.decorateAsFloatingWindow = xplm_WindowDecorationNone;

    overlayWindow = XPLMCreateWindowEx(&params);
    if (!overlayWindow) {
        debug("Could not create the notification overlay for app '%s'.\n", id.c_str());
        return;
    }

    XPLMSetWindowIsVisible(overlayWindow, 0);
}

void App::registerChromeWindows() {
    // Plain OpenGL windows with no decoration of X-Plane's: they draw the bars
    // themselves and own every click inside their bounds. Placed by
    // syncChromeGeometry, which keeps them on the browser window's edges.
    XPLMCreateWindow_t params = {};
    params.structSize = kLegacyWindowStructSize;
    params.left = 0;
    params.top = titleBarHeight;
    params.right = 1;
    params.bottom = 0;
    params.visible = 0;
    params.refcon = this;
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationSelfDecorated;
    params.handleRightClickFunc = consumeClick;
    params.handleMouseWheelFunc = consumeWheel;
    params.handleKeyFunc = ignoreKey;

    params.drawWindowFunc = [](XPLMWindowID inWindowID, void *inRefcon) {
        static_cast<App*>(inRefcon)->drawTitleBar();
    };
    params.handleMouseClickFunc = [](XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus status, void *inRefcon) -> int {
        return static_cast<App*>(inRefcon)->handleTitleBarClick(x, y, status);
    };
    params.handleCursorFunc = [](XPLMWindowID inWindowID, int x, int y, void *inRefcon) -> XPLMCursorStatus {
        return static_cast<App*>(inRefcon)->titleBarCursor(x, y);
    };
    titleWindow = XPLMCreateWindowEx(&params);

    params.top = resizeFooterHeight;
    params.drawWindowFunc = [](XPLMWindowID inWindowID, void *inRefcon) {
        static_cast<App*>(inRefcon)->drawFooter();
    };
    params.handleMouseClickFunc = [](XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus status, void *inRefcon) -> int {
        return static_cast<App*>(inRefcon)->handleFooterClick(x, y, status);
    };
    params.handleCursorFunc = [](XPLMWindowID inWindowID, int x, int y, void *inRefcon) -> XPLMCursorStatus {
        return static_cast<App*>(inRefcon)->footerCursor(x, y);
    };
    footerWindow = XPLMCreateWindowEx(&params);

    if (!titleWindow || !footerWindow) {
        debug("Could not create the window chrome for app '%s'.\n", id.c_str());
        return;
    }

    syncChromeGeometry();
}

bool App::usesChrome() const {
    if (!window || !titleWindow || !footerWindow) {
        return false;
    }

    return Dataref::getInstance()->getCached<int>("sim/graphics/VR/enabled") == 0;
}

void App::updateChromeVisibility() {
    if (!titleWindow || !footerWindow) {
        return;
    }

    int wanted = (visible && usesChrome()) ? 1 : 0;
    if (wanted) {
        syncChromeGeometry();
    }
    if (XPLMGetWindowIsVisible(titleWindow) != wanted) {
        XPLMSetWindowIsVisible(titleWindow, wanted);
    }
    if (XPLMGetWindowIsVisible(footerWindow) != wanted) {
        XPLMSetWindowIsVisible(footerWindow, wanted);
    }
}

void App::syncChromeGeometry() {
    if (!window || !titleWindow || !footerWindow) {
        return;
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    auto place = [](XPLMWindowID chrome, int l, int t, int r, int b) {
        int cl, ct, cr, cb;
        XPLMGetWindowGeometry(chrome, &cl, &ct, &cr, &cb);
        if (cl != l || ct != t || cr != r || cb != b) {
            XPLMSetWindowGeometry(chrome, l, t, r, b);
        }
    };

    place(titleWindow, left, top + titleBarHeight, right, top);
    place(footerWindow, left, bottom, right, bottom - resizeFooterHeight);
}

void App::setViewportFromWindow(XPLMWindowID aWindow) {
    int left, top, right, bottom;
    XPLMGetWindowGeometry(aWindow, &left, &top, &right, &bottom);
    setViewport(left, bottom, static_cast<unsigned short>((std::max)(1, right - left)), static_cast<unsigned short>((std::max)(1, top - bottom)));
}

// A real bar: opaque, lighter than the page, and closed off by a rule -- the
// same three things every window manager uses to say "this strip is not the
// document". The whole bar except the close button is the drag region.
void App::drawTitleBar() {
    if (!visible || !usesChrome()) {
        return;
    }

    App::current = this;
    // Every frame, so the bar follows X-Plane's own edge resizing.
    syncChromeGeometry();
    setViewportFromWindow(titleWindow);
    static bool titleLogged = false;
    logGlStateOnce("Title bar", titleWindow, &titleLogged);

    float opacity = std::clamp(config.window_opacity, 0.2f, 1.0f);
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    glColor4f(0.105f, 0.125f, 0.15f, opacity * 0.94f);
    Drawing::DrawRect(0.0f, 0.0f, 1.0f, 1.0f);

    // The bar's own bottom row of pixels, so the boundary reads as a rule.
    glColor4f(0.95f, 0.97f, 1.0f, 0.16f);
    Drawing::DrawRect(0.0f, 0.0f, 1.0f, 1.0f / viewport.height);

    // Close button: an "x" centred in its square at the bar's left end.
    float centreX = (closeButtonSize * 0.5f) / viewport.width;
    float halfX = 4.5f / viewport.width;
    float halfY = 4.5f / viewport.height;
    glColor4f(0.82f, 0.87f, 0.93f, 0.9f);
    Drawing::DrawLine(centreX - halfX, 0.5f - halfY, centreX + halfX, 0.5f + halfY, 1.5f);
    Drawing::DrawLine(centreX - halfX, 0.5f + halfY, centreX + halfX, 0.5f - halfY, 1.5f);

    if (!name.empty()) {
        // DrawText centres on x, so the left edge of the string is the inset
        // and the x passed is the inset plus half the width.
        float inset = static_cast<float>(closeButtonSize + 6) / viewport.width;
        float textWidth = Drawing::TextWidth(name);
        Drawing::DrawText(name, inset + textWidth / 2.0f, 0.5f, 1.0f, { 0.82f, 0.87f, 0.93f });
    }

    App::current = nullptr;
}

// The footer exists so the grip has somewhere to live that is not on top of
// the page.
void App::drawFooter() {
    if (!visible || !usesChrome()) {
        return;
    }

    App::current = this;
    syncChromeGeometry();
    setViewportFromWindow(footerWindow);
    static bool footerLogged = false;
    logGlStateOnce("Footer", footerWindow, &footerLogged);

    float opacity = std::clamp(config.window_opacity, 0.2f, 1.0f);
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    glColor4f(0.105f, 0.125f, 0.15f, opacity * 0.94f);
    Drawing::DrawRect(0.0f, 0.0f, 1.0f, 1.0f);

    glColor4f(0.95f, 0.97f, 1.0f, 0.16f);
    Drawing::DrawRect(0.0f, 1.0f - (1.0f / viewport.height), 1.0f, 1.0f);

    // Three stacked diagonals: the grip every desktop toolkit draws, so it
    // needs no label to be understood.
    float right = 1.0f - (5.0f / viewport.width);
    float bottom = 3.0f / viewport.height;
    float span = 9.0f;

    glColor4f(0.95f, 0.97f, 1.0f, 0.55f);
    for (int line = 0; line < 3; ++line) {
        float offset = line * 4.0f;
        float x1 = right - (span - offset) / viewport.width;
        float y2 = bottom + (span - offset) / viewport.height;
        Drawing::DrawLine(x1, bottom, right, y2, 1.5f);
    }

    App::current = nullptr;
}

bool App::isCloseButtonPoint(int x, int y) const {
    if (!titleWindow) {
        return false;
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(titleWindow, &left, &top, &right, &bottom);
    return x >= left && x < left + closeButtonSize && y >= bottom && y <= top;
}

bool App::isResizeGripPoint(int x, int y) const {
    if (!footerWindow) {
        return false;
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(footerWindow, &left, &top, &right, &bottom);
    return x > right - resizeGripSize && x <= right && y >= bottom && y <= top;
}

int App::handleTitleBarClick(int x, int y, XPLMMouseStatus status) {
    if (!visible || !window) {
        return 0;
    }

    if (status == xplm_MouseDown) {
        if (isCloseButtonPoint(x, y)) {
            closeButtonPressed = true;
            return 1;
        }

        XPLMGetWindowGeometry(window, &dragLeft, &dragTop, &dragRight, &dragBottom);
        dragStartX = x;
        dragStartY = y;
        windowDragActive = true;
        XPLMBringWindowToFront(window);
        XPLMBringWindowToFront(titleWindow);
        XPLMBringWindowToFront(footerWindow);
        return 1;
    }

    if (status == xplm_MouseDrag && windowDragActive) {
        int deltaX = x - dragStartX;
        int deltaY = y - dragStartY;
        XPLMSetWindowGeometry(window, dragLeft + deltaX, dragTop + deltaY, dragRight + deltaX, dragBottom + deltaY);
        syncChromeGeometry();
        return 1;
    }

    if (status == xplm_MouseUp) {
        // Closing on release, and only if the release is still on the button,
        // is what lets a user change their mind by dragging off it.
        bool close = closeButtonPressed && isCloseButtonPoint(x, y);
        closeButtonPressed = false;
        windowDragActive = false;
        if (close) {
            hideBrowser();
        }
        return 1;
    }

    return 1;
}

int App::handleFooterClick(int x, int y, XPLMMouseStatus status) {
    if (!visible || !window) {
        return 0;
    }

    if (status == xplm_MouseDown) {
        if (!isResizeGripPoint(x, y)) {
            return 1;
        }

        XPLMGetWindowGeometry(window, &dragLeft, &dragTop, &dragRight, &dragBottom);
        dragStartX = x;
        dragStartY = y;
        windowResizeActive = true;
        return 1;
    }

    if (status == xplm_MouseDrag && windowResizeActive) {
        // Bottom-right grip: the left and top edges are the anchor, so only the
        // right and bottom move and the window does not walk across the screen.
        int right = dragRight + (x - dragStartX);
        int bottom = dragBottom + (y - dragStartY);

        // The same floor XPLMSetWindowResizingLimits enforces for X-Plane's own
        // edge handles, applied here because we set the geometry directly.
        int minimumWidth = (std::max)(static_cast<int>(config.minimum_width), minimumContentWidth);
        if (right - dragLeft < minimumWidth) {
            right = dragLeft + minimumWidth;
        }
        if (dragTop - bottom < minimumContentHeight) {
            bottom = dragTop - minimumContentHeight;
        }

        XPLMSetWindowGeometry(window, dragLeft, dragTop, right, bottom);
        syncChromeGeometry();
        return 1;
    }

    if (status == xplm_MouseUp) {
        windowResizeActive = false;
    }

    return 1;
}

XPLMCursorStatus App::titleBarCursor(int x, int y) {
    return isCloseButtonPoint(x, y) ? xplm_CursorButton : xplm_CursorDefault;
}

XPLMCursorStatus App::footerCursor(int x, int y) {
    return (windowResizeActive || isResizeGripPoint(x, y)) ? xplm_CursorFourArrows : xplm_CursorDefault;
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
