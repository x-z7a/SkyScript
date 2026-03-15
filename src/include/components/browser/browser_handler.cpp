// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "browser_handler.h"

#include "app.h"
#include "config.h"
#include "notification.h"
#include "path.h"

#include <cmath>
#include <include/base/cef_callback.h>
#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_parser.h>
#include <include/views/cef_browser_view.h>
#include <include/views/cef_window.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/wrapper/cef_helpers.h>
#include <sstream>
#include <string>
#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

BrowserHandler::BrowserHandler(int aTextureId, std::string *aCurrentUrl, unsigned short aWidth, unsigned short aHeight) {
    textureId = aTextureId;
    popupRect = {0, 0, 0, 0};
    popupShown = false;
    needsFullDraw = true;
    currentUrl = aCurrentUrl;
    windowWidth = aWidth;
    windowHeight = aHeight;
    cursorState = CursorDefault;
    hasInputFocus = false;
    browserInstance = nullptr;
}

BrowserHandler::~BrowserHandler() {
    textureId = 0;
    browserInstance = nullptr;
    cursorState = CursorDefault;
    hasInputFocus = false;
}

void BrowserHandler::destroy() {
    textureId = 0;
    popupShown = false;
    needsFullDraw = true;
    cursorState = CursorDefault;
    hasInputFocus = false;
}

void BrowserHandler::setViewSize(unsigned short width, unsigned short height) {
    windowWidth = width;
    windowHeight = height;
    popupRect = {0, 0, 0, 0};
    popupShown = false;
    needsFullDraw = true;
}

void BrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    browserInstance = browser;
    if (App::current) {
        browserInstance->GetHost()->SetAudioMuted(App::current->config.audio_muted);
    }
}

bool BrowserHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    textureId = 0;
    return false;
}

void BrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    textureId = 0;
    browserInstance = nullptr;
}

bool BrowserHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &target_url, const CefString &target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture, const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client, CefBrowserSettings &settings, CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) {
    if (user_gesture && !target_url.empty()) {
        browser->GetMainFrame()->LoadURL(target_url);
    }

    return true;
}

void BrowserHandler::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
    popupShown = show;

    if (popupShown) {
        browser->GetHost()->Invalidate(PET_POPUP);
    } else {
        needsFullDraw = true;
    }
}

void BrowserHandler::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect &rect) {
    popupRect = {rect.x, rect.y, rect.width, rect.height};
}

void BrowserHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
    rect = CefRect(0, 0, windowWidth, windowHeight);
}

void BrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title) {
    (void)browser;
    (void)title;
}

void BrowserHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height) {
    if (!textureId) {
        return;
    }

    XPLMBindTexture2d(textureId, 0);
    constexpr uint32_t bytes_per_pixel = 4;

    for (const auto &rect : dirtyRects) {
        const uint8_t *rectBuffer = static_cast<const uint8_t *>(buffer) + rect.y * width * bytes_per_pixel + rect.x * bytes_per_pixel;

        glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

        if (needsFullDraw) {
            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0, 0,
                width, height,
                GL_BGRA,
                GL_UNSIGNED_BYTE,
                buffer);
            needsFullDraw = false;
        } else if (popupShown) {
            if (type == PET_POPUP) {
                glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    popupRect.x + rect.x, popupRect.y + rect.y,
                    rect.width, rect.height,
                    GL_BGRA,
                    GL_UNSIGNED_BYTE,
                    rectBuffer);
            }
        } else {
            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                rect.x, rect.y,
                rect.width, rect.height,
                GL_BGRA,
                GL_UNSIGNED_BYTE,
                rectBuffer);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

bool BrowserHandler::OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor, cef_cursor_type_t type, const CefCursorInfo &custom_cursor_info) {
    switch (type) {
        case CT_HAND:
            cursorState = CursorHand;
            break;

        case CT_IBEAM:
        case CT_VERTICALTEXT:
            cursorState = CursorText;
            break;

        default:
            cursorState = CursorDefault;
    }

    return false;
}

void BrowserHandler::OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser, TextInputMode input_mode) {
    hasInputFocus = input_mode != CEF_TEXT_INPUT_MODE_NONE;
}

void BrowserHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {
    (void)canGoBack;
    (void)canGoForward;

    if (!isLoading) {
        *currentUrl = browser->GetMainFrame()->GetURL().ToString();
    }
}

void BrowserHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString &errorText, const CefString &failedUrl) {
    CEF_REQUIRE_UI_THREAD();
    if (errorCode == ERR_ABORTED) {
        return;
    }

#if DEBUG
    if (failedUrl.ToString() == "http://__debug__/") {
        const std::string htmlString = R"(
        <html>
        <head>
            <meta charset="UTF-8" />
            <meta name="viewport" content="width=device-width, initial-scale=1.0" />
            <script src="https://unpkg.com/@tailwindcss/browser@4"></script>
            <title>)" + std::string(FRIENDLY_NAME) +
                                       R"(</title>
            <script>
                function refreshUserAgent() {
                    document.getElementById('user-agent').textContent = navigator.userAgent;
                }
                
                function didLoad() {
                    refreshUserAgent();
                
                    navigator.geolocation.watchPosition(({coords, wind, extra}) => {
                        document.getElementById('location').textContent = `${coords.latitude}, ${coords.longitude} at ${coords.altitude}m msl, ${extra.altitudeAgl}m agl - ${coords.speed}m/s`;
                        document.getElementById('wind').textContent = `${wind.direction}deg / ${wind.speedKts}kts - Airspeed: ${extra.airspeedKts}kts`;
                    });
                }
                
                if (document.readyState === "complete") {
                    didLoad();
                }
                else {
                    window.addEventListener("load", didLoad);
                }
            </script>
        </head>
        <body class="flex flex-col items-center justify-start w-full">
            <div class="flex flex-col items-center gap-4 max-w-3xl">
                <h1 class="text-3xl font-bold underline">)" +
                                       std::string(FRIENDLY_NAME) + R"(</h1>
                <div>
                    <input id="alert-text" placeholder="Type here" />
                    <button onclick="javascript:alert(document.getElementById('alert-text').value || 'Test');">Show alert</button>
                </div>
        
                <div>
                    <select class="w-48 p-2 border border-gray-300 rounded-lg shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                        <option>Option 1</option>
                        <option>Option 2</option>
                        <option>Option 3</option>
                        <option>Option 4</option>
                        <option>Option 5</option>
                        <option>Option 6</option>
                        <option>Option 7</option>
                        <option>Option 8</option>
                        <option>Option 9</option>
                        <option>Option 10</option>
                        <option>Option 11</option>
                        <option>Option 12</option>
                        <option>Option 13</option>
                        <option>Option 14</option>
                        <option>Option 15</option>
                        <option>Option 16</option>
                        <option>Option 17</option>
                        <option>Option 18</option>
                        <option>Option 19</option>
                        <option>Option 20</option>
                    </select>
                </div>
        
                <div class="flex flex-col w-full gap-4">
                    <div class="flex flex-col gap-2 text-xs" onclick="refreshUserAgent();">
                        <span>User-Agent (JS)</span>
                        <span id="user-agent">not_loaded</span>
                    </div>
            
                    <div class="flex flex-col gap-2 text-xs">
                        <span>Location</span>
                        <span id="location">not_loaded</span>
                    </div>
            
                    <div class="flex flex-col gap-2 text-xs">
                        <span>Wind</span>
                        <span id="wind">not_loaded</span>
                    </div>
                </div>
            </div>
        </body>
        </html>
        )";
        browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8," + htmlString);
        return;
    }
#endif

    debug("Error loading %s: %s\n", failedUrl.ToString().c_str(), errorText.ToString().c_str());
}

bool BrowserHandler::OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString &origin_url, JSDialogType dialog_type, const CefString &message_text, const CefString &default_prompt_text, CefRefPtr<CefJSDialogCallback> callback, bool &suppress_message) {
    suppress_message = true;

    if (App::current) {
        App::current->showNotification(new Notification("Alert", message_text.ToString()));
    }
    return false;
}

bool BrowserHandler::OnFileDialog(CefRefPtr<CefBrowser> browser, FileDialogMode mode, const CefString &title, const CefString &default_file_path, const std::vector<CefString> &accept_filters, CefRefPtr<CefFileDialogCallback> callback) {
    // debug("file dialog: %i :: %s", mode, title.ToString().c_str());
    return false;
}

bool BrowserHandler::OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefPermissionPromptCallback> callback) {
    if (requested_permissions & CEF_PERMISSION_TYPE_GEOLOCATION) {
        //        callback->Continue(CEF_PERMISSION_RESULT_DENY);
        //        return true;
        return false;
    }

    debug("Denied browser permissions request from %s. Requested flags=%i\n", requesting_origin.ToString().c_str(), requested_permissions);
    callback->Continue(CEF_PERMISSION_RESULT_DENY);

    return true;
}

bool BrowserHandler::OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefMediaAccessCallback> callback) {
    callback->Continue(CEF_MEDIA_PERMISSION_NONE);
    return true;
}

void BrowserHandler::OnDocumentAvailableInMainFrame(CefRefPtr<CefBrowser> browser) {
    if (!browser->GetMainFrame()) {
        return;
    }

    overrideGeolocationAndNavigator(browser);
}

void BrowserHandler::OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString &suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback) {
    if (suggested_name == "b738x.xml" || suggested_name.ToString().ends_with(".fms")) {
        std::string filename = Path::getInstance()->rootDirectory + "/output/FMS plans/" + suggested_name.ToString();
        callback->Continue(filename, false);
        return;
    }

    // Cancel all other downloads (by default).
    if (App::current) {
        App::current->showNotification(new Notification("Download failed", "Could not download the requested file."));
    }
}

void BrowserHandler::OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback) {
    if (download_item->IsComplete()) {
        if (App::current) {
            App::current->showNotification(new Notification("Download finished", "The download has been completed."));
        }
    }
}

cef_return_value_t BrowserHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) {
    CefRequest::HeaderMap headers;
    request->GetHeaderMap(headers);
    auto it = headers.find("User-Agent");
    if (it == headers.end()) {
        return RV_CONTINUE;
    }

    headers.erase("User-Agent");
    if (App::current) {
        headers.insert(std::make_pair("User-Agent", App::current->config.user_agent));
    }
    request->SetHeaderMap(headers);
    return RV_CONTINUE;
}

#if DEBUG
bool BrowserHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) {
    if (frame->IsMain()) {
        std::string url = request->GetURL();
        debug("URL: %s\n", url.c_str());
    }

    return false;
}
#endif

void BrowserHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
    (void)httpStatusCode;

    if (!frame->IsMain()) {
        return;
    }

    overrideGeolocationAndNavigator(browser);
}

void BrowserHandler::overrideGeolocationAndNavigator(CefRefPtr<CefBrowser> browser) {
    std::string userAgent = App::current ? App::current->config.user_agent : "Mozilla/5.0";

    std::string javascript =
        "function setUserAgent(window, userAgent) {"
        "    try {"
        "        var userAgentProp = Object.getOwnPropertyDescriptor(navigator, 'userAgent');"
        "        if (userAgentProp && userAgentProp.configurable) {"
        "            Object.defineProperty(navigator, 'userAgent', {"
        "                get: function () { return userAgent; },"
        "                configurable: true"
        "            });"
        "        } else if (navigator.__defineGetter__) {"
        "            navigator.__defineGetter__('userAgent', function () {"
        "                return userAgent;"
        "            });"
        "        }"
        "    } catch (e) {}"
        "}"
        "window.skyscript_watchers = (window.skyscript_watchers || {});"
        "Object.defineProperty(navigator, 'onLine', {"
        "    get: function() { return true; },"
        "    configurable: true"
        "});"
        "navigator.permissions.query = (options) => {"
        "    return Promise.resolve({ state: 'granted' });"
        "};"
        "navigator.geolocation.watchPosition = (success, error, options) => {"
        "    window.skyscript_watchers = (window.skyscript_watchers || {});"
        "    const id = Math.round(Date.now() / 1000);"
        "    window.skyscript_watchers[id] = success;"
        "    if (window.skyscript_location) { success(window.skyscript_location); }"
        "    return id;"
        "};"
        "navigator.geolocation.clearWatch = (id) => {"
        "    if (!window.skyscript_watchers) { return; }"
        "    delete window.skyscript_watchers[id];"
        "};"
        "navigator.geolocation.getCurrentPosition = (success, error, options) => {"
        "    if (window.skyscript_location) {"
        "        success(window.skyscript_location);"
        "    } else {"
        "        const wid = navigator.geolocation.watchPosition(success, error, options);"
        "    }"
        "};"
        "setUserAgent(window, \"" +
        userAgent + "\");"
                    "window.dispatchEvent(new Event('load'));";

    browser->GetMainFrame()->ExecuteJavaScript(javascript.c_str(), browser->GetMainFrame()->GetURL(), 0);
}
