#include "cef_app.h"
#include "cef_manager.h"
#include "manager.h"

#ifdef SKYSCRIPT_CEF_ENABLED

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/wrapper/cef_helpers.h"

#include <cstring>

// ── CefApp ────────────────────────────────────────────────────────────────────

CefApp::CefApp() : AppBase() {}

CefApp::CefApp(const std::string &name, const std::string &dir)
    : AppBase(name, dir)
{
}

CefApp::~CefApp()
{
    Destroy();
}

bool CefApp::IsInitialized() const
{
    return initialized_ && browser_ != nullptr;
}

void CefApp::Initialize()
{
    LogMsg("CefApp::Initialize: %s", app_name_.c_str());

    if (!CefManager::instance().IsInitialized())
    {
        LogMsg("[%s] CefApp::Initialize: CefManager not initialized — aborting", app_name_.c_str());
        return;
    }

    // Build the file URL for this app's index.html.
    // CEF uses the standard file:// scheme with an absolute path.
    std::string plugin_dir = Manager::instance().getPluginDir();
    std::string index_path = plugin_dir + "/apps/" + app_name_ + "/index.html";

#if defined(_WIN32)
    // Windows: file:///C:/path/to/index.html
    std::replace(index_path.begin(), index_path.end(), '\\', '/');
    std::string url = "file:///" + index_path;
#else
    std::string url = "file://" + index_path;
#endif

    LogMsg("[%s] CefApp loading URL: %s", app_name_.c_str(), url.c_str());

    client_ = new CefAppClient(this);

    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr); // OSR: no native window handle

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;

    // CreateBrowser is asynchronous; OnAfterCreated() fires when it's ready.
    CefBrowserHost::CreateBrowser(window_info, client_, url,
                                  browser_settings, nullptr, nullptr);

    // CreateXPlaneWindow is called here so the window is ready immediately.
    // The browser may not have rendered its first frame yet.
    CreateXPlaneWindow();
    initialized_ = true;
}

void CefApp::Destroy()
{
    if (browser_)
    {
        LogMsg("[%s] CefApp::Destroy: closing browser", app_name_.c_str());
        browser_->GetHost()->CloseBrowser(true);
        // browser_ will be released in OnBeforeClose callback
    }

    if (main_window_)
    {
        XPLMDestroyWindow(main_window_);
        main_window_ = nullptr;
    }
    if (texture_id_ != 0)
    {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }

    client_     = nullptr;
    initialized_ = false;
}

void CefApp::OnWindowResized(int new_width, int new_height)
{
    if (browser_)
    {
        browser_->GetHost()->WasResized();
    }
    // pixel_buffer_ will be updated by the next OnPaint() callback
}

// ── Texture upload ────────────────────────────────────────────────────────────

void CefApp::UpdateTexture()
{
    if (!has_new_frame_ || pixel_buffer_.empty())
        return;

    has_new_frame_ = false;

    if (texture_id_ == 0)
    {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
    }

    // CEF delivers BGRA, same as Ultralight — no conversion needed.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 paint_width_, paint_height_,
                 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 pixel_buffer_.data());
}

void CefApp::ForceRepaint()
{
    if (browser_)
        browser_->GetHost()->Invalidate(PET_VIEW);
}

void CefApp::Reload()
{
    if (browser_)
    {
        LogMsg("[%s] CefApp::Reload", app_name_.c_str());
        browser_->Reload();
    }
}

// ── Input routing (Phase 6) ───────────────────────────────────────────────────

int CefApp::OnMouseClick(int x, int y, int button, int mouseStatus)
{
    if (!browser_ || !main_window_)
        return 0;

    bool isDown = (mouseStatus == xplm_MouseDown);
    bool isUp   = (mouseStatus == xplm_MouseUp);
    bool isDrag = (mouseStatus == xplm_MouseDrag);

    if (isDown)
        XPLMTakeKeyboardFocus(main_window_);

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    // X-Plane: origin bottom-left.  CEF: origin top-left.
    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    CefMouseEvent evt;
    evt.x = view_x;
    evt.y = view_y;
    evt.modifiers = 0;

    cef_mouse_button_type_t btn = (button == 0) ? MBT_LEFT : MBT_RIGHT;

    if (isDown)
        browser_->GetHost()->SendMouseClickEvent(evt, btn, false, 1);
    else if (isUp)
        browser_->GetHost()->SendMouseClickEvent(evt, btn, true, 1);
    else if (isDrag)
        browser_->GetHost()->SendMouseMoveEvent(evt, false);

    return 1;
}

int CefApp::OnMouseMove(int x, int y)
{
    if (!browser_ || !main_window_)
        return 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    CefMouseEvent evt;
    evt.x = view_x;
    evt.y = view_y;
    evt.modifiers = 0;

    browser_->GetHost()->SendMouseMoveEvent(evt, false);
    return 1;
}

int CefApp::OnMouseWheel(int x, int y, int /*wheel*/, int clicks)
{
    if (!browser_ || !main_window_)
        return 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    CefMouseEvent evt;
    evt.x = view_x;
    evt.y = view_y;
    evt.modifiers = 0;

    browser_->GetHost()->SendMouseWheelEvent(evt, 0, clicks * 30);
    return 1;
}

void CefApp::OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus)
{
    if (!browser_)
        return;

    if (losingFocus)
        return;

    bool isDown = (flags & xplm_DownFlag) != 0;

    CefKeyEvent evt;
    evt.windows_key_code  = static_cast<int>(static_cast<unsigned char>(virtualKey));
    evt.native_key_code   = static_cast<int>(static_cast<unsigned char>(virtualKey));
    evt.modifiers         = 0;
    if (flags & xplm_ShiftFlag)     evt.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (flags & xplm_OptionAltFlag) evt.modifiers |= EVENTFLAG_ALT_DOWN;
    if (flags & xplm_ControlFlag)   evt.modifiers |= EVENTFLAG_CONTROL_DOWN;

    if (isDown)
    {
        evt.type = KEYEVENT_RAWKEYDOWN;
        browser_->GetHost()->SendKeyEvent(evt);

        if (key >= 32 && key < 127)
        {
            evt.type       = KEYEVENT_CHAR;
            evt.character  = static_cast<char16_t>(key);
            browser_->GetHost()->SendKeyEvent(evt);
        }
    }
    else
    {
        evt.type = KEYEVENT_KEYUP;
        browser_->GetHost()->SendKeyEvent(evt);
    }
}

// ── SkyScriptRenderHandler ────────────────────────────────────────────────────

void SkyScriptRenderHandler::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect &rect)
{
    rect.Set(0, 0, owner_->view_width_, owner_->view_height_);
}

void SkyScriptRenderHandler::OnPaint(CefRefPtr<CefBrowser> /*browser*/,
                                      PaintElementType type,
                                      const RectList & /*dirtyRects*/,
                                      const void *buffer,
                                      int width, int height)
{
    if (type != PET_VIEW)
        return;

    // Copy the BGRA pixel buffer.  buffer is owned by CEF and only valid
    // during this callback, so we copy it.
    size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    owner_->pixel_buffer_.resize(bytes);
    std::memcpy(owner_->pixel_buffer_.data(), buffer, bytes);
    owner_->paint_width_   = width;
    owner_->paint_height_  = height;
    owner_->has_new_frame_ = true;
}

// ── CefAppClient ──────────────────────────────────────────────────────────────

CefAppClient::CefAppClient(CefApp *owner)
    : owner_(owner)
    , render_handler_(new SkyScriptRenderHandler(owner))
{
}

void CefAppClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();
    owner_->browser_ = browser;
    LogMsg("[%s] CefAppClient: browser created (id=%d)",
           owner_->app_name_.c_str(), browser->GetIdentifier());
}

bool CefAppClient::DoClose(CefRefPtr<CefBrowser> /*browser*/)
{
    // Return false to allow the default close behavior
    return false;
}

void CefAppClient::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/)
{
    CEF_REQUIRE_UI_THREAD();
    LogMsg("[%s] CefAppClient: browser closed", owner_->app_name_.c_str());
    owner_->browser_ = nullptr;
}

void CefAppClient::OnLoadStart(CefRefPtr<CefBrowser> /*browser*/,
                                CefRefPtr<CefFrame> frame,
                                TransitionType /*transition_type*/)
{
    if (frame->IsMain())
        LogMsg("[%s] CefApp: loading started", owner_->app_name_.c_str());
}

void CefAppClient::OnLoadEnd(CefRefPtr<CefBrowser> /*browser*/,
                              CefRefPtr<CefFrame> frame,
                              int httpStatusCode)
{
    if (frame->IsMain())
        LogMsg("[%s] CefApp: loading finished (status=%d)",
               owner_->app_name_.c_str(), httpStatusCode);
    // TODO (Phase 7): inject XPlane / SkyScript JS bindings via CefFrame::ExecuteJavaScript
}

void CefAppClient::OnLoadError(CefRefPtr<CefBrowser> /*browser*/,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString &errorText,
                                const CefString &failedUrl)
{
    if (frame->IsMain())
        LogMsg("[%s] CefApp: loading FAILED url=%s error=%s (code=%d)",
               owner_->app_name_.c_str(),
               failedUrl.ToString().c_str(),
               errorText.ToString().c_str(),
               static_cast<int>(errorCode));
}

bool CefAppClient::OnConsoleMessage(CefRefPtr<CefBrowser> /*browser*/,
                                     cef_log_severity_t /*level*/,
                                     const CefString &message,
                                     const CefString &source,
                                     int line)
{
    LogMsg("[%s] CEF Console: %s (source: %s, line: %d)",
           owner_->app_name_.c_str(),
           message.ToString().c_str(),
           source.ToString().c_str(),
           line);
    return false; // don't suppress default CEF logging
}

#endif // SKYSCRIPT_CEF_ENABLED
