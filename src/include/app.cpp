#include "app.h"

#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include "browser.h"
#include "button.h"
#include "config.h"
#include "dataref.h"
#include "drawing.h"
#include "notification.h"
#include "path.h"

namespace {
unsigned short nextPowerOfTwo(unsigned int value) {
    unsigned int result = 1;
    while (result < value) {
        result <<= 1;
    }

    return static_cast<unsigned short>((std::min)(result, static_cast<unsigned int>(std::numeric_limits<unsigned short>::max())));
}
}

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
    windowDragActive = false;
    windowDragStartX = 0;
    windowDragStartY = 0;
    windowDragLeft = 0;
    windowDragTop = 0;
    windowDragRight = 0;
    windowDragBottom = 0;
    windowResizeActive = false;
    windowResizeStartX = 0;
    windowResizeStartY = 0;
    windowResizeLeft = 0;
    windowResizeTop = 0;
    windowResizeRight = 0;
    windowResizeBottom = 0;
    visible = false;
    mouseDown = false;
    browser = nullptr;
    activeCursor = CursorDefault;
    brightness = 1.0f;
    // The viewport is a window size, and the configured size is a page size.
    unsigned short initWidth = config.width > 0 ? config.width : defaultWindowWidth;
    unsigned short initHeight = static_cast<unsigned short>(
        (config.height > 0 ? config.height : defaultWindowHeight) + static_cast<int>(chromeBoxels()));
    viewport = {0, 0, initWidth, initHeight, 0, 0, 0, 0};
}

App::~App() {
    deinitialize();
}

void App::initialize() {
    App::current = this;

    unsigned short initWidth = config.width > 0 ? config.width : defaultWindowWidth;
    unsigned short initHeight = static_cast<unsigned short>(
        (config.height > 0 ? config.height : defaultWindowHeight) + static_cast<int>(chromeBoxels()));
    setViewport(0, 0, initWidth, initHeight);

    if (!browser) {
        browser = new Browser();
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

    Dataref::getInstance()->createCommand((datarefPrefix + "/devtools").c_str(), ("Toggle developer tools for " + name).c_str(), [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (browser) {
            browser->showDevTools();
        }
    });

    App::current = nullptr;
}

void App::deinitialize() {
    App::current = this;

    std::string datarefPrefix = "skyscript/" + id;
    Dataref::getInstance()->unbind((datarefPrefix + "/visible").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/toggle").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/devtools").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/url").c_str());
    Dataref::getInstance()->unbind((datarefPrefix + "/refresh").c_str());

    clearNotification();

    if (browser) {
        browser->visibilityWillChange(false);
        browser->destroy();
        browser = nullptr;
    }

    if (window) {
        XPLMDestroyWindow(window);
        window = nullptr;
    }

    tasks.clear();
    buttons.clear();
    windowDragActive = false;
    notificationOverlay = false;
    visible = false;
    activeCursor = CursorDefault;
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

    if (visible) {
        syncWindowGeometry();
    }
    else if (notificationOverlay) {
        // The toast lays itself out against the viewport, so it still needs
        // real geometry. The browser is hidden, so it must not be resized to
        // match: that would rasterise a page nobody is looking at.
        syncWindowGeometry(false);
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

void App::draw() {
    if (!visible && !notificationOverlay) {
        return;
    }

    App::current = this;

    syncWindowGeometry(visible);

    // With the browser hidden the window exists only to carry the toast, so
    // neither the window background nor the page is painted -- what the user
    // sees is the notification alone, floating over the cockpit.
    if (visible) {
        drawWindowBackground();
        browser->draw();
    }

    if (notification) {
        notification->draw();
    }

    App::current = nullptr;
}

bool App::syncWindowGeometry(bool resizeBrowser) {
    if (!window) {
        return false;
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    unsigned short width = static_cast<unsigned short>((std::max)(1, right - left));
    unsigned short height = static_cast<unsigned short>((std::max)(1, top - bottom));
    bool changed = setViewport(left, bottom, width, height);
    // Pushed every sync rather than only on change: the chrome is a fixed number
    // of boxels, so its share of the window moves with every resize, and both
    // drawing and input read these. Every input path reaches here through
    // normalizeWindowPoint, which is what keeps a click mapped to the same pixel
    // the page drew.
    if (browser) {
        browser->setPageExtent(pageBottomRatio(), pageTopRatio());
    }
    if (changed && resizeBrowser && browser) {
        browser->resize();
    }

    return changed;
}

bool App::normalizeWindowPoint(int x, int y, float *normalizedX, float *normalizedY) {
    syncWindowGeometry(false);

    if (viewport.width == 0 || viewport.height == 0) {
        return false;
    }

    *normalizedX = static_cast<float>(x - viewport.x) / viewport.width;
    *normalizedY = static_cast<float>(y - viewport.y) / viewport.height;

    return !(*normalizedX < -0.1f || *normalizedX > 1.1f || *normalizedY < -0.1f || *normalizedY > 1.1f);
}

void App::applyWindowMode() {
    if (!window) {
        return;
    }

    bool isVrEnabled = Dataref::getInstance()->get<bool>("sim/graphics/VR/enabled");
    XPLMSetWindowPositioningMode(window, isVrEnabled ? xplm_WindowVR : xplm_WindowPositionFree, -1);

    if (visible) {
        XPLMBringWindowToFront(window);
    }
}

bool App::updateButtons(float normalizedX, float normalizedY, ButtonState state) {
    bool didAct = false;
    for (const auto& button : buttons) {
        didAct = didAct || button->handleState(normalizedX, normalizedY, state);
    }

    return didAct;
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

void App::drawWindowBackground() {
    if (!config.window_titleless) {
        return;
    }

    XPLMSetGraphicsState(
        0,
        0,
        0,
        0,
        1,
        0,
        0);

    glColor4f(0.02f, 0.025f, 0.03f, std::clamp(config.window_opacity, 0.2f, 1.0f) * 0.18f);
    Drawing::DrawRoundedRect(0.0f, 0.0f, 1.0f, 1.0f, 8.0f);

    drawTitleBar();
    drawResizeFooter();
}

// A real bar rather than the translucent highlight this used to be. It reads as
// chrome because it is opaque, lighter than the page behind it, and closed off
// by a rule -- the same three things every window manager uses to say "this
// strip is not the document". The whole bar is the drag region.
void App::drawTitleBar() {
    if (viewport.height == 0) {
        return;
    }

    float opacity = std::clamp(config.window_opacity, 0.2f, 1.0f);
    float top = 1.0f;
    float bottom = pageTopRatio();

    glColor4f(0.105f, 0.125f, 0.15f, opacity * 0.94f);
    Drawing::DrawRect(0.0f, bottom, 1.0f, top);

    // The bar's own bottom row of pixels, so the boundary reads as a rule
    // without the bar losing height to it.
    glColor4f(0.95f, 0.97f, 1.0f, 0.16f);
    Drawing::DrawRect(0.0f, bottom, 1.0f, bottom + (1.0f / viewport.height));

    if (name.empty()) {
        return;
    }

    // Clears the back/close button, which sits at the bar's left end in both
    // address-bar modes. DrawText centres on x, so the left edge of the string
    // is the inset and the x passed is the inset plus half the width.
    float inset = 34.0f / viewport.width;
    float textWidth = Drawing::TextWidth(name);
    float centreY = bottom + (titleBarBoxels() * 0.5f) / viewport.height;
    Drawing::DrawText(name, inset + textWidth / 2.0f, centreY, 1.0f, { 0.82f, 0.87f, 0.93f });
}

// The footer exists so the grip has somewhere to live that is not on top of the
// page. It is the same band as the title bar, which also gives the window a
// bottom edge to sit on rather than the page running into the rounded corner.
void App::drawResizeFooter() {
    if (viewport.height == 0 || viewport.width == 0) {
        return;
    }

    // Re-asserted because drawTitleBar may have drawn the app name just before
    // this, and XPLMDrawString leaves the texturing state set up for glyphs.
    XPLMSetGraphicsState(
        0,
        0,
        0,
        0,
        1,
        0,
        0);

    float opacity = std::clamp(config.window_opacity, 0.2f, 1.0f);
    float top = pageBottomRatio();

    glColor4f(0.105f, 0.125f, 0.15f, opacity * 0.94f);
    Drawing::DrawRect(0.0f, 0.0f, 1.0f, top);

    glColor4f(0.95f, 0.97f, 1.0f, 0.16f);
    Drawing::DrawRect(0.0f, top - (1.0f / viewport.height), 1.0f, top);

    // Three stacked diagonals: the grip every desktop toolkit draws, so it needs
    // no label to be understood.
    float right = 1.0f - (5.0f / viewport.width);
    float bottom = 3.0f / viewport.height;
    float span = (resizeGripSize - 5.0f);

    glColor4f(0.95f, 0.97f, 1.0f, 0.55f);
    for (int line = 0; line < 3; ++line) {
        float offset = line * 4.0f;
        float x1 = right - (span - offset) / viewport.width;
        float y1 = bottom;
        float x2 = right;
        float y2 = bottom + (span - offset) / viewport.height;
        Drawing::DrawLine(x1, y1, x2, y2, 1.5f);
    }
}

bool App::isWindowDragRegion(float normalizedX, float normalizedY) const {
    if (!config.window_titleless || !window || viewport.height == 0) {
        return false;
    }

    bool isVrEnabled = Dataref::getInstance()->getCached<int>("sim/graphics/VR/enabled") != 0;
    if (isVrEnabled) {
        return false;
    }

    // The whole title bar drags, which it can now afford to be: it is no longer
    // a strip stolen from the top of the page, so widening it costs the app
    // nothing.
    return normalizedX >= 0.0f && normalizedX <= 1.0f && normalizedY >= pageTopRatio() && normalizedY <= 1.0f;
}

bool App::beginWindowDrag(float normalizedX, float normalizedY, int x, int y) {
    if (!isWindowDragRegion(normalizedX, normalizedY)) {
        return false;
    }

    XPLMGetWindowGeometry(window, &windowDragLeft, &windowDragTop, &windowDragRight, &windowDragBottom);
    windowDragStartX = x;
    windowDragStartY = y;
    windowDragActive = true;
    mouseDown = true;

    if (browser) {
        browser->setFocus(false);
    }
    XPLMTakeKeyboardFocus(0);

    return true;
}

bool App::dragWindow(int x, int y) {
    if (!windowDragActive || !window) {
        return false;
    }

    int deltaX = x - windowDragStartX;
    int deltaY = y - windowDragStartY;
    XPLMSetWindowGeometry(
        window,
        windowDragLeft + deltaX,
        windowDragTop + deltaY,
        windowDragRight + deltaX,
        windowDragBottom + deltaY);
    syncWindowGeometry(false);

    return true;
}

void App::endWindowDrag() {
    windowDragActive = false;
    mouseDown = false;
}

bool App::isResizeGripRegion(float normalizedX, float normalizedY) const {
    if (!config.window_titleless || !window || viewport.height == 0 || viewport.width == 0) {
        return false;
    }

    bool isVrEnabled = Dataref::getInstance()->getCached<int>("sim/graphics/VR/enabled") != 0;
    if (isVrEnabled) {
        return false;
    }

    // A square in the footer's right end, slightly larger than the drawn
    // diagonals so it is reachable without pixel-hunting.
    float left = 1.0f - (resizeGripSize + 6.0f) / viewport.width;
    float top = (resizeGripSize + 2.0f) / viewport.height;
    return normalizedX >= left && normalizedX <= 1.0f && normalizedY >= 0.0f && normalizedY <= top;
}

bool App::beginWindowResize(float normalizedX, float normalizedY, int x, int y) {
    if (!isResizeGripRegion(normalizedX, normalizedY)) {
        return false;
    }

    XPLMGetWindowGeometry(window, &windowResizeLeft, &windowResizeTop, &windowResizeRight, &windowResizeBottom);
    windowResizeStartX = x;
    windowResizeStartY = y;
    windowResizeActive = true;
    mouseDown = true;

    if (browser) {
        browser->setFocus(false);
    }
    XPLMTakeKeyboardFocus(0);

    return true;
}

bool App::resizeWindow(int x, int y) {
    if (!windowResizeActive || !window) {
        return false;
    }

    // Bottom-right grip: the left and top edges are the anchor, so only the
    // right and bottom move and the window does not walk across the screen as
    // it is resized.
    int right = windowResizeRight + (x - windowResizeStartX);
    int bottom = windowResizeBottom + (y - windowResizeStartY);

    // The same floor XPLMSetWindowResizingLimits enforces for X-Plane's own
    // edge handles, applied here because we are setting the geometry directly.
    int minimumWidth = (std::max)(static_cast<int>(config.minimum_width), minimumContentWidth);
    int minimumHeight = minimumContentHeight + static_cast<int>(chromeBoxels());
    if (right - windowResizeLeft < minimumWidth) {
        right = windowResizeLeft + minimumWidth;
    }
    if (windowResizeTop - bottom < minimumHeight) {
        bottom = windowResizeTop - minimumHeight;
    }

    XPLMSetWindowGeometry(window, windowResizeLeft, windowResizeTop, right, bottom);
    syncWindowGeometry(true);

    return true;
}

void App::endWindowResize() {
    windowResizeActive = false;
    mouseDown = false;
}

bool App::isResizingWindow() const {
    return windowResizeActive;
}

bool App::isDraggingWindow() const {
    return windowDragActive;
}

void App::showBrowser(std::string url) {
    if (!browser || !window) {
        return;
    }

    if (!url.empty()) {
        browser->loadUrl(url);
    }

    // The window may already be on screen carrying a toast. Opening the app
    // takes it over rather than raising a second one.
    notificationOverlay = false;

    if (!visible) {
        browser->visibilityWillChange(true);
        visible = true;
        XPLMSetWindowIsVisible(window, 1);
        applyWindowMode();
        syncWindowGeometry();
    }

    XPLMBringWindowToFront(window);
}

void App::hideBrowser() {
    if (!visible) {
        return;
    }

    browser->visibilityWillChange(false);
    visible = false;
    browser->setFocus(false);
    XPLMTakeKeyboardFocus(0);

    if (window) {
        XPLMSetWindowIsVisible(window, 0);
    }

    // Closing the app mid-toast should not swallow it. The window comes back
    // immediately in notification-only mode and the toast finishes its life.
    if (notification) {
        setNotificationOverlay(true);
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
    // the whole point of a notification. With the browser hidden the window
    // comes up carrying nothing but the toast.
    setNotificationOverlay(true);
}

void App::showNotification(const NotificationOptions& options) {
    App* previous = App::current;
    App::current = this;

    NotificationOptions resolved = options;
    if (canRaiseNotificationOverlay()) {
        // Over a hidden browser the window passes every click through to the
        // simulator, so nothing on the toast can be clicked. Drawing a close
        // button there would offer an affordance that does nothing, and a
        // notification that waits to be dismissed would then never leave.
        resolved.dismissible = false;
        if (resolved.timeoutSeconds <= 0.0f) {
            resolved.timeoutSeconds = Notification::defaultOptions().timeoutSeconds;
        }
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

// setNotificationOverlay puts the X-Plane window on screen without opening the
// app, or takes it away again.
//
// `visible` keeps its meaning throughout: it is whether the browser is shown.
// Every input handler already refuses to act when it is false, so an overlay
// window is click-through by construction and cannot steal a cockpit click --
// which is the property that makes showing it at all acceptable.
void App::setNotificationOverlay(bool active) {
    if (notificationOverlay == active) {
        return;
    }

    if (active && !canRaiseNotificationOverlay()) {
        return;
    }

    notificationOverlay = active;

    if (!window) {
        return;
    }

    if (active) {
        XPLMSetWindowIsVisible(window, 1);
        XPLMBringWindowToFront(window);
        syncWindowGeometry(false);
    }
    else if (!visible) {
        XPLMSetWindowIsVisible(window, 0);
    }
}

bool App::hasNotificationOverlay() const {
    return notificationOverlay;
}

// A notification can bring its own window only if the app draws its own chrome.
// Decoration is fixed when the window is created, so a titled app cannot drop
// its frame for one toast -- showing its window would put an empty X-Plane
// panel on screen with a notification inside it. Those apps keep the older
// behaviour, where notifications appear while the app is open, rather than
// getting a worse-looking version of the new one.
bool App::canRaiseNotificationOverlay() const {
    return !visible && config.window_titleless;
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

float App::titleBarBoxels() const {
    return config.window_titleless ? titleBarHeight : 0.0f;
}

float App::footerBoxels() const {
    return config.window_titleless ? resizeFooterHeight : 0.0f;
}

float App::chromeBoxels() const {
    return titleBarBoxels() + footerBoxels();
}

float App::pageBottomRatio() const {
    if (viewport.height == 0) {
        return 0.0f;
    }

    return footerBoxels() / viewport.height;
}

float App::pageTopRatio() const {
    if (viewport.height == 0) {
        return 1.0f;
    }

    return 1.0f - (titleBarBoxels() / viewport.height);
}

bool App::setViewport(int x, int y, unsigned short width, unsigned short height) {
    unsigned short safeWidth = (std::max<unsigned short>)(1, width);
    unsigned short safeHeight = (std::max<unsigned short>)(1, height);
    float multiplier = safeWidth < config.minimum_width ? static_cast<float>(config.minimum_width) / safeWidth : 1.0f;
    unsigned short browserWidth = static_cast<unsigned short>(std::ceil(safeWidth * multiplier));
    // The page gets the window less the chrome. registerWindow grows the window
    // by the same amount, so a manifest asking for 1024x768 rasterises 1024x768
    // of page rather than 768 minus whatever we drew on top of it.
    unsigned short chrome = static_cast<unsigned short>(chromeBoxels());
    unsigned short contentHeight = safeHeight > chrome ? static_cast<unsigned short>(safeHeight - chrome) : 1;
    unsigned short browserHeight = static_cast<unsigned short>((std::max)(1.0f, std::ceil(contentHeight * multiplier)));
    unsigned short textureWidth = nextPowerOfTwo(browserWidth);
    unsigned short textureHeight = nextPowerOfTwo(browserHeight);

    bool changed = viewport.x != x ||
        viewport.y != y ||
        viewport.width != safeWidth ||
        viewport.height != safeHeight ||
        viewport.textureWidth != textureWidth ||
        viewport.textureHeight != textureHeight ||
        viewport.browserWidth != browserWidth ||
        viewport.browserHeight != browserHeight;

    viewport = {
        x,
        y,
        safeWidth,
        safeHeight,
        textureWidth,
        textureHeight,
        browserWidth,
        browserHeight
    };

    return changed;
}

void App::registerWindow() {
    if (window) {
        XPLMDestroyWindow(window);
        window = nullptr;
    }

    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);

    float screenWidth = fabs(winLeft - winRight);
    float screenHeight = fabs(winTop - winBot);
    int width = config.width > 0 ? config.width : defaultWindowWidth;
    // The manifest asks for a page size, so the window is that plus the chrome.
    // Reserving the chrome out of the requested height instead would silently
    // shrink every existing app's page by 44 boxels.
    int height = (config.height > 0 ? config.height : defaultWindowHeight) + static_cast<int>(chromeBoxels());

    XPLMCreateWindow_t params;
    params.structSize = sizeof(params);
    params.left = (int)(winLeft + (screenWidth - width) / 2);
    params.right = params.left + width;
    params.top = (int)(winTop - (screenHeight - height) / 2);
    params.bottom = params.top - height;
    params.visible = 0;
    params.refcon = this;
    params.drawWindowFunc = [](XPLMWindowID inWindowID, void *inRefcon) {
        App *app = static_cast<App*>(inRefcon);
        app->draw();
    };
    params.handleMouseClickFunc = [](XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus status, void* inRefcon) -> int {
        App *app = static_cast<App*>(inRefcon);
        App::current = app;

        if (!app->visible) {
            App::current = nullptr;
            return 0;
        }

        if (app->isDraggingWindow()) {
            if (status == xplm_MouseDrag) {
                app->dragWindow(x, y);
            }
            else if (status == xplm_MouseUp) {
                app->endWindowDrag();
            }
            App::current = nullptr;
            return 1;
        }

        if (app->isResizingWindow()) {
            if (status == xplm_MouseDrag) {
                app->resizeWindow(x, y);
            }
            else if (status == xplm_MouseUp) {
                app->endWindowResize();
            }
            App::current = nullptr;
            return 1;
        }

        float mouseX, mouseY;
        if (!app->normalizeWindowPoint(x, y, &mouseX, &mouseY)) {
            if (app->browser->hasInputFocus()) {
                app->browser->setFocus(false);
            }
            App::current = nullptr;
            return 0;
        }

        if (status == xplm_MouseDown && app->updateButtons(mouseX, mouseY, kButtonClick)) {
            App::current = nullptr;
            return 1;
        }

        // Before the drag test: the grip sits in the footer and the title bar
        // owns the drag, so the two never overlap, but ordering it this way
        // keeps that true if either region ever grows.
        if (status == xplm_MouseDown && app->beginWindowResize(mouseX, mouseY, x, y)) {
            App::current = nullptr;
            return 1;
        }

        if (status == xplm_MouseDown && app->beginWindowDrag(mouseX, mouseY, x, y)) {
            App::current = nullptr;
            return 1;
        }

        if (status == xplm_MouseDown) {
            app->mouseDown = true;
        }
        else if (status == xplm_MouseUp) {
            app->mouseDown = false;
        }

        if (app->browser->click(status, mouseX, mouseY)) {
            App::current = nullptr;
            return 1;
        }

        app->mouseDown = false;
        app->browser->setFocus(false);
        App::current = nullptr;
        return 0;
    };
    params.handleRightClickFunc = nullptr;
    params.handleMouseWheelFunc = [](XPLMWindowID inWindowID, int x, int y, int wheel, int clicks, void* inRefcon) -> int {
        App *app = static_cast<App*>(inRefcon);
        App::current = app;

        if (!app->visible) {
            App::current = nullptr;
            return 0;
        }

        float mouseX, mouseY;
        if (!app->normalizeWindowPoint(x, y, &mouseX, &mouseY)) {
            App::current = nullptr;
            return 0;
        }

        bool horizontal = wheel == 1;
        app->browser->scroll(mouseX, mouseY, clicks * app->config.scroll_speed, horizontal);
        App::current = nullptr;
        return 1;
    };
    params.handleKeyFunc = [](XPLMWindowID inWindowID, char key, XPLMKeyFlags flags, char virtualKey, void* inRefcon, int losingFocus) {
        App *app = static_cast<App*>(inRefcon);
        App::current = app;

        if (!app->visible) {
            App::current = nullptr;
            return;
        }

        if ((flags & xplm_DownFlag) == xplm_DownFlag) {
            app->browser->key(key, virtualKey, flags);
        }

        if ((flags & xplm_UpFlag) == xplm_UpFlag) {
            app->browser->key(key, virtualKey, flags);
        }

        if (losingFocus) {
            app->browser->setFocus(false);
        }

        App::current = nullptr;
    };
    params.handleCursorFunc = [](XPLMWindowID inWindowID, int x, int y, void* inRefcon) -> XPLMCursorStatus {
        App *app = static_cast<App*>(inRefcon);
        App::current = app;

        // The other input handlers already stand down when the browser is
        // hidden; this one did not need to, because a hidden app had no window
        // for the simulator to route the cursor to. A notification-only window
        // does, and without this the pointer would drive a page nobody can see
        // and pick up hover cursors over empty cockpit.
        if (!app->visible) {
            app->activeCursor = CursorDefault;
            App::current = nullptr;
            return xplm_CursorDefault;
        }

        float mouseX, mouseY;
        if (!app->normalizeWindowPoint(x, y, &mouseX, &mouseY)) {
            app->activeCursor = CursorDefault;
            App::current = nullptr;
            return xplm_CursorDefault;
        }

        if (!app->mouseDown) {
            app->browser->mouseMove(mouseX, mouseY);
        }

        bool isVREnabled = Dataref::getInstance()->getCached<int>("sim/graphics/VR/enabled");
        if (isVREnabled) {
            App::current = nullptr;
            return xplm_CursorDefault;
        }

        CursorType wantedCursor = CursorDefault;
        if (app->isResizeGripRegion(mouseX, mouseY)) {
            wantedCursor = CursorResize;
        }
        else if (app->updateButtons(mouseX, mouseY, kButtonHover)) {
            wantedCursor = CursorHand;
        }
        else if (app->visible && app->browser->cursor() != CursorDefault) {
            wantedCursor = app->browser->cursor();
        }

        if (wantedCursor == CursorDefault) {
            app->activeCursor = CursorDefault;
            App::current = nullptr;
            return xplm_CursorDefault;
        }

        if (wantedCursor != app->activeCursor) {
            app->activeCursor = wantedCursor;
            setCursor(wantedCursor);
        }

        App::current = nullptr;
        return xplm_CursorCustom;
    };
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = config.window_titleless ? xplm_WindowDecorationSelfDecoratedResizable : xplm_WindowDecorationRoundRectangle;

    window = XPLMCreateWindowEx(&params);
    if (!config.window_titleless) {
        XPLMSetWindowTitle(window, name.c_str());
    }
    // The floor is a *page* floor, so it moves with the chrome: 480 of window
    // would otherwise mean 436 of page once the bars are taken out.
    XPLMSetWindowResizingLimits(
        window,
        minimumContentWidth,
        minimumContentHeight + static_cast<int>(chromeBoxels()),
        4096,
        4096);
    syncWindowGeometry(false);
    applyWindowMode();
    XPLMSetWindowIsVisible(window, 0);
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
