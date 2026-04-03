#include "browser.h"

#include "app.h"
#include "browser_handler.h"
#include "config.h"
#include "dataref.h"
#include "drawing.h"
#include "notification.h"
#include "path.h"
#include "xplm_bridge.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <include/base/cef_bind.h>
#include <include/base/cef_callback.h>
#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_command_line.h>
#include <include/cef_render_handler.h>
#include <include/cef_request_context_handler.h>
#include <include/cef_version.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/wrapper/cef_helpers.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#if APL
#include "unix_keycodes.h"

#include <include/wrapper/cef_library_loader.h>
#elif LIN
#include "unix_keycodes.h"
#endif

Browser::Browser() {
    textureId = 0;
    offsetStart = 0.0f;
    offsetEnd = App::browserTopRatio;
    lastGpsUpdateTime = 0.0f;
    backButton = nullptr;
    xplmBridge = new XplmBridge();
    handler = nullptr;
    currentUrl = "";
}

void Browser::initialize() {
    if (textureId || handler) {
        return;
    }

    App *app = App::current;

    offsetStart = 0.0f;
    offsetEnd = App::browserTopRatio;

    std::string icon = app->config.hide_addressbar ? "/icons/arrow-left-circle.svg" : "/icons/x-circle.svg";
    backButton = new Button(Path::getInstance()->assetsDirectory + icon);
    backButton->setPosition(backButton->relativeWidth / 2.0f + 0.01f, App::toolbarY);
    backButton->setClickHandler([app]() {
        if (!app->visible) {
            return false;
        }

        if (!app->config.hide_addressbar) {
            app->hideBrowser();
            return true;
        }

        bool didGoBack = app->browser->goBack();
        if (!didGoBack) {
            app->hideBrowser();
        }

        return true;
    });

    currentUrl = app->config.homepage;
    allocateTexture();

    std::string datarefPrefix = "skyscript/" + app->id;
    Dataref::getInstance()->createDataref<std::string>((datarefPrefix + "/url").c_str(), &currentUrl, true, [this](std::string newUrl) {
        if (!newUrl.starts_with("http") && !newUrl.starts_with("chrome://") && !newUrl.starts_with("data:") && !newUrl.starts_with("file://")) {
            return false;
        }

        loadUrl(newUrl);
        return true;
    });

    Dataref::getInstance()->createCommand((datarefPrefix + "/refresh").c_str(), "Refresh the current web page", [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        if (handler && handler->browserInstance) {
            handler->browserInstance->Reload();
        }
    });

    createBrowser();
}

void Browser::allocateTexture() {
    if (!textureId) {
        XPLMGenerateTextureNumbers(&textureId, 1);
    }

    XPLMBindTexture2d(textureId, 0);

    const auto &viewport = App::current->viewport;
    std::vector<unsigned char> whiteTextureData(
        viewport.textureWidth * viewport.textureHeight * WindowViewport::bytesPerPixel,
        0xFF);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        viewport.textureWidth,
        viewport.textureHeight,
        0,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        whiteTextureData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Browser::destroy() {
    if (handler) {
        if (handler->browserInstance) {
            handler->browserInstance->GetHost()->CloseBrowser(true);

            auto startTime = std::chrono::steady_clock::now() + std::chrono::seconds(99);
            auto gracePeriod = std::chrono::milliseconds(500);
            while (1) {
                CefDoMessageLoopWork();

                if (!handler->browserInstance && startTime > std::chrono::steady_clock::now()) {
                    startTime = std::chrono::steady_clock::now();
                } else if (std::chrono::steady_clock::now() - startTime > gracePeriod) {
                    break;
                }
            }
        }

        handler->destroy();
        handler = nullptr;
    }

    if (textureId) {
        XPLMBindTexture2d(textureId, 0);
        glDeleteTextures(1, (GLuint *) &textureId);
        textureId = 0;
    }

    if (backButton) {
        backButton->destroy();
        backButton = nullptr;
    }

    if (xplmBridge) {
        delete xplmBridge;
        xplmBridge = nullptr;
    }
}

void Browser::visibilityWillChange(bool becomesVisible) {
    lastGpsUpdateTime = becomesVisible ? XPLMGetElapsedTime() : 0.0f;
}

void Browser::update() {
    if (!textureId) {
        return;
    }

    App *app = App::current;

    if (handler) {
        CefDoMessageLoopWork();
    }

    if (xplmBridge) {
        xplmBridge->processPendingRequests();
    }

    if (backButton) {
        backButton->visible = app->visible;
    }

    if (lastGpsUpdateTime > std::numeric_limits<float>::epsilon() && XPLMGetElapsedTime() > lastGpsUpdateTime + 1.0f) {
        updateGPSLocation();
    }
}

void Browser::draw() {
    if (!textureId) {
        return;
    }

    XPLMSetGraphicsState(
        0,
        1,
        0,
        0,
        1,
        0,
        0);

    XPLMBindTexture2d(textureId, 0);

    const auto &viewport = App::current->viewport;
    int x1 = viewport.x;
    int y1 = viewport.y + viewport.height * offsetStart;
    int x2 = x1 + viewport.width;
    int y2 = viewport.y + viewport.height * offsetEnd;

    glBegin(GL_QUADS);
    set_brightness(App::current->brightness);

    float u = (float) viewport.browserWidth / viewport.textureWidth;
    float v = (float) viewport.browserHeight / viewport.textureHeight;

    glTexCoord2f(0, v);
    glVertex2f(x1, y1);
    glTexCoord2f(0, 0);
    glVertex2f(x1, y2);
    glTexCoord2f(u, 0);
    glVertex2f(x2, y2);
    glTexCoord2f(u, v);
    glVertex2f(x2, y1);
    glEnd();

    if (backButton) {
        backButton->draw();
    }
}

void Browser::resize() {
    if (!textureId) {
        return;
    }

    allocateTexture();

    if (!handler) {
        return;
    }

    const auto &viewport = App::current->viewport;
    handler->setViewSize(viewport.browserWidth, viewport.browserHeight);
    if (handler->browserInstance) {
        handler->browserInstance->GetHost()->WasResized();
        handler->browserInstance->GetHost()->Invalidate(PET_VIEW);
    }
}

void Browser::mouseMove(float normalizedX, float normalizedY) {
    if (!textureId || !handler || !handler->browserInstance) {
        return;
    }

    if (normalizedX < 0 || normalizedX > 1 || normalizedY < offsetStart || normalizedY > offsetEnd) {
        return;
    }

    CefMouseEvent mouseEvent = getMouseEvent(normalizedX, normalizedY);
    handler->browserInstance->GetHost()->SendMouseMoveEvent(mouseEvent, false);
}

bool Browser::click(XPLMMouseStatus status, float normalizedX, float normalizedY) {
    if (!textureId || !handler || !handler->browserInstance) {
        return false;
    }

    if (normalizedX < 0 || normalizedX > 1 || normalizedY < offsetStart || normalizedY > offsetEnd) {
        return false;
    }

    CefMouseEvent mouseEvent = getMouseEvent(normalizedX, normalizedY);
    if (mouseEvent.y < 0) {
        return false;
    }

    if (status == xplm_MouseDown) {
        handler->browserInstance->GetHost()->SendMouseClickEvent(mouseEvent, MBT_LEFT, false, 1);
    } else if (status == xplm_MouseDrag) {
        mouseEvent.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
        handler->browserInstance->GetHost()->SendMouseMoveEvent(mouseEvent, false);
    } else {
        handler->browserInstance->GetHost()->SendMouseClickEvent(mouseEvent, MBT_LEFT, true, 1);
    }

    return true;
}

void Browser::scroll(float normalizedX, float normalizedY, int clicks, bool horizontal) {
    if (!textureId || !handler || !handler->browserInstance) {
        return;
    }

    if (normalizedX < 0 || normalizedX > 1 || normalizedY < offsetStart || normalizedY > offsetEnd) {
        return;
    }

    CefMouseEvent mouseEvent = getMouseEvent(normalizedX, normalizedY);
    mouseEvent.modifiers = EVENTFLAG_NONE;
    handler->browserInstance->GetHost()->SendMouseWheelEvent(mouseEvent, horizontal ? clicks : 0, horizontal ? 0 : clicks);
}

void Browser::loadUrl(std::string url) {
    if (!textureId || !handler) {
        currentUrl = url;
        return;
    }

    currentUrl = url;
    if (handler->browserInstance) {
        handler->browserInstance->GetMainFrame()->LoadURL(url);
    }
}

bool Browser::hasInputFocus() {
    if (!textureId || !handler) {
        return false;
    }

    return handler->hasInputFocus;
}

void Browser::setFocus(bool focus) {
    if (!textureId || !handler || !handler->browserInstance) {
        return;
    }

    handler->browserInstance->GetHost()->SetFocus(focus);
    if (!focus && handler->hasInputFocus) {
        std::string script = "document.activeElement?.blur();";
        handler->browserInstance->GetMainFrame()->ExecuteJavaScript(script, handler->browserInstance->GetMainFrame()->GetURL(), 0);
    }
}

void Browser::key(unsigned char key, unsigned char virtualKey, XPLMKeyFlags flags) {
    if (!textureId || !handler || !handler->browserInstance) {
        return;
    }

    CefKeyEvent keyEvent;
    keyEvent.type = (flags == 0 || (flags & xplm_DownFlag) == xplm_DownFlag) ? KEYEVENT_KEYDOWN : KEYEVENT_KEYUP;

#if IBM
    wchar_t utf16Character;
    MultiByteToWideChar(CP_UTF8, 0, (char *) &key, 1, &utf16Character, 1);
    keyEvent.windows_key_code = virtualKey;
    keyEvent.native_key_code = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    keyEvent.character = utf16Character;
    keyEvent.unmodified_character = keyEvent.character;
#else
    auto it = virtualKeycodeToUnixKeycode.find(virtualKey);
    if (it != virtualKeycodeToUnixKeycode.end()) {
        int keyCode = it->second;
        keyEvent.native_key_code = keyCode;
    } else {
        debug("Unknown key: 0x%02X VK: 0x%02X\n", key, virtualKey);
        keyEvent.native_key_code = key;
    }
    keyEvent.windows_key_code = virtualKey;
    keyEvent.character = key;
    keyEvent.unmodified_character = keyEvent.character;
#endif

    keyEvent.is_system_key = false;
    keyEvent.modifiers = 0;
    if ((flags & xplm_ShiftFlag) == xplm_ShiftFlag) {
        keyEvent.modifiers |= EVENTFLAG_SHIFT_DOWN;
    }

    if ((flags & xplm_OptionAltFlag) == xplm_OptionAltFlag) {
        keyEvent.modifiers |= EVENTFLAG_ALT_DOWN;
    }

    if ((flags & xplm_ControlFlag) == xplm_ControlFlag) {
        keyEvent.modifiers |= EVENTFLAG_CONTROL_DOWN;

        if (key == 'a') {
            if (keyEvent.type == KEYEVENT_KEYDOWN) {
                handler->browserInstance->GetMainFrame()->SelectAll();
            }
            return;
        } else if (key == 'c') {
            if (keyEvent.type == KEYEVENT_KEYDOWN) {
                handler->browserInstance->GetMainFrame()->Copy();
            }
            return;
        } else if (key == 'v') {
            if (keyEvent.type == KEYEVENT_KEYDOWN) {
                handler->browserInstance->GetMainFrame()->Paste();
            }
            return;
        }
    }

    handler->browserInstance->GetHost()->SendKeyEvent(keyEvent);

    if (keyEvent.type == KEYEVENT_KEYDOWN && isprint(key)) {
        CefKeyEvent textEvent;
        textEvent.type = KEYEVENT_CHAR;
        textEvent.character = keyEvent.character;
        textEvent.unmodified_character = keyEvent.unmodified_character;
        textEvent.native_key_code = keyEvent.native_key_code;
        textEvent.windows_key_code = keyEvent.character;

        handler->browserInstance->GetHost()->SendKeyEvent(textEvent);
    }
}

bool Browser::goBack() {
    if (!textureId || !handler || !handler->browserInstance) {
        return false;
    }

    if (!handler->browserInstance->CanGoBack()) {
        return false;
    }

    handler->browserInstance->GoBack();
    return true;
}

void Browser::showDevTools() {
    if (!handler || !handler->browserInstance) {
        return;
    }

    if (handler->browserInstance->GetHost()->HasDevTools()) {
        handler->browserInstance->GetHost()->CloseDevTools();
        return;
    }

    CefWindowInfo windowInfo;
    CefBrowserSettings settings;
    CefPoint inspectAt;
    handler->browserInstance->GetHost()->ShowDevTools(windowInfo, nullptr, settings, inspectAt);
}

CursorType Browser::cursor() {
    if (!handler) {
        return CursorDefault;
    }

    return handler->cursorState;
}

bool Browser::createBrowser() {
    if (handler && handler->browserInstance) {
        return false;
    }

    App *app = App::current;

#if APL
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInMain()) {
        debug("Could not load the CEF framework.\n");
        return false;
    }
#endif

    std::string cachePath = Path::getInstance()->pluginDirectory + "/cache";
    if (!std::filesystem::exists(cachePath)) {
        std::filesystem::create_directories(cachePath);
    }

    CefRequestContextSettings context_settings;
    CefString(&context_settings.cache_path) = cachePath;

    std::string language = "";
    switch (XPLMLanguageCode()) {
        case xplm_Language_English:
            language = "en-US,en";
            break;
        case xplm_Language_French:
            language = "fr-FR,fr";
            break;
        case xplm_Language_German:
            language = "de-DE,de";
            break;
        case xplm_Language_Italian:
            language = "it-IT,it";
            break;
        case xplm_Language_Spanish:
            language = "es-ES,es";
            break;
        case xplm_Language_Korean:
            language = "ko-KR,ko";
            break;
        case xplm_Language_Russian:
            language = "ru-RU,ru";
            break;
        case xplm_Language_Greek:
            language = "el-GR,el";
            break;
        case xplm_Language_Japanese:
            language = "ja-JP,ja";
            break;
        case xplm_Language_Chinese:
            language = "zh-CN,zh";
            break;
        case xplm_Language_Unknown:
        default:
            break;
    }

    if (!app->config.forced_language.empty()) {
        language = app->config.forced_language;
    }

    if (!language.empty()) {
        CefString(&context_settings.accept_language_list) = language;
    }

    context_settings.persist_user_preferences = true;
    context_settings.persist_session_cookies = true;
    CefRefPtr<CefRequestContext> request_context = CefRequestContext::CreateContext(context_settings, nullptr);

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = app->config.framerate;
    browser_settings.background_color = CefColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);

    const auto &viewport = app->viewport;
    handler = CefRefPtr<BrowserHandler>(new BrowserHandler(textureId, &currentUrl, &app->config, xplmBridge, app->name, viewport.browserWidth, viewport.browserHeight));

    CefWindowInfo window_info;
#if LIN
    window_info.SetAsWindowless(0);
#else
    window_info.SetAsWindowless(nullptr);
#endif
    window_info.windowless_rendering_enabled = true;

    bool browserCreated = CefBrowserHost::CreateBrowser(window_info, handler, currentUrl, browser_settings, nullptr, request_context);
    if (!browserCreated) {
        app->showNotification(new Notification("Error creating browser", "An error occured while starting the browser.\nPlease verify if there are any updates for the " FRIENDLY_NAME " plugin and try again."));
    }
    debug("Browser for app '%s' created: %d\n", app->id.c_str(), browserCreated);
    return true;
}

void Browser::updateGPSLocation() {
    if (!handler || !handler->browserInstance) {
        return;
    }

    float latitude = Dataref::getInstance()->get<float>("sim/flightmodel/position/latitude");
    float longitude = Dataref::getInstance()->get<float>("sim/flightmodel/position/longitude");
    float speedMetersSecond = Dataref::getInstance()->get<float>("sim/flightmodel/position/groundspeed");
    float altitudeMetersAboveSeaLevel = Dataref::getInstance()->get<float>("sim/flightmodel/position/elevation");
    float magneticHeading = Dataref::getInstance()->get<float>("sim/flightmodel/position/mag_psi");

    float windDirection = Dataref::getInstance()->get<float>("sim/weather/wind_direction_degt");
    float windSpeed = Dataref::getInstance()->get<float>("sim/weather/wind_speed_kt");

    float altitudeMetersAboveGroundLevel = Dataref::getInstance()->get<float>("sim/flightmodel/position/y_agl");
    float airspeedKts = Dataref::getInstance()->get<float>("sim/flightmodel/position/indicated_airspeed");

    std::stringstream stream;
    stream << "window.skyscript_location = { ";
    stream << "coords: { ";
    stream << "latitude: " << std::fixed << std::setprecision(6) << latitude << ", ";
    stream << "longitude: " << std::fixed << std::setprecision(6) << longitude << ", ";
    stream << "accuracy: 10, ";
    stream << "altitude: " << std::fixed << std::setprecision(0) << altitudeMetersAboveSeaLevel << ", ";
    stream << "altitudeAccuracy: 10, ";
    stream << "heading: " << std::fixed << std::setprecision(0) << magneticHeading << ", ";
    stream << "speed: " << std::fixed << std::setprecision(0) << speedMetersSecond << ", ";
    stream << "}, ";
    stream << "wind: { ";
    stream << "direction: " << std::fixed << std::setprecision(0) << windDirection << ", ";
    stream << "speedKts: " << std::fixed << std::setprecision(0) << windSpeed << ", ";
    stream << "}, ";
    stream << "extra: { ";
    stream << "altitudeAgl: " << std::fixed << std::setprecision(0) << altitudeMetersAboveGroundLevel << ", ";
    stream << "airspeedKts: " << std::fixed << std::setprecision(0) << airspeedKts << ", ";
    stream << "}, timestamp: Date.now() }; for (let key in window.skyscript_watchers) { window.skyscript_watchers[key](window.skyscript_location); }";

    handler->browserInstance->GetMainFrame()->ExecuteJavaScript(stream.str(), handler->browserInstance->GetMainFrame()->GetURL(), 0);
    lastGpsUpdateTime = XPLMGetElapsedTime();
}

CefMouseEvent Browser::getMouseEvent(float normalizedX, float normalizedY) {
    const auto &viewport = App::current->viewport;

    CefMouseEvent mouseEvent;
    mouseEvent.x = viewport.browserWidth * normalizedX;
    mouseEvent.y = viewport.browserHeight * (1.0f - ((normalizedY - offsetStart) / (offsetEnd - offsetStart)));
    return mouseEvent;
}

void Browser::onMessage(const std::string& channel, MessageHandler handler) {
    if (xplmBridge) {
        xplmBridge->onMessage(channel, handler);
    }
}

void Browser::postMessage(const std::string& channel, const std::string& payload) {
    if (xplmBridge) {
        xplmBridge->postMessage(channel, payload);
    }
}
