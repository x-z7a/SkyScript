#include "ultralight_app.h"
#include "bindings/bindings.h"
#include "manager.h"

#include <cstring>
#include <filesystem>

// Set to 1 to enable debug logging and screenshot saving
#define DEBUG_DRAW 0

UltralightApp::UltralightApp() : AppBase() {}

UltralightApp::UltralightApp(const std::string &name, const std::string &dir)
    : AppBase(name, dir)
{
}

UltralightApp::~UltralightApp()
{
    Destroy();
}

// ── Initialize / Destroy ──────────────────────────────────────────────────────

void UltralightApp::Initialize()
{
    LogMsg("UltralightApp::Initialize: %s", app_name_.c_str());

    renderer_ = Manager::instance().renderer_;
    if (!renderer_)
    {
        LogMsg("[%s] Cannot initialize - Ultralight renderer not ready", app_name_.c_str());
        return;
    }

    // Per-app session for isolated cookies, localStorage, indexedDB
    std::string app_cache_dir = Manager::instance().getOutputDir() + "/cache/" + app_name_;
    std::filesystem::create_directories(app_cache_dir);

    session_ = renderer_->CreateSession(true, app_name_.c_str());
    LogMsg("[%s] Created session: %s (persistent=%d, path=%s)",
           app_name_.c_str(),
           session_->name().utf8().data(),
           session_->is_persistent(),
           session_->disk_path().utf8().data());

    main_view_ = renderer_->CreateView(view_width_, view_height_, ViewConfig(), session_);
    main_view_->set_view_listener(this);
    main_view_->set_load_listener(this);

    // Load index.html using Ultralight's FileSystem (plugin_dir is the base)
    std::string file_url = "file:///apps/" + app_name_ + "/index.html";
    LogMsg("[%s] Loading URL: %s", app_name_.c_str(), file_url.c_str());
    main_view_->LoadURL(file_url.c_str());

    CreateXPlaneWindow();
}

void UltralightApp::Destroy()
{
    // Inspector first
    if (inspector_window_)
    {
        LogMsg("[%s] Destroying inspector window", app_name_.c_str());
        XPLMDestroyWindow(inspector_window_);
        inspector_window_ = nullptr;
    }
    if (inspector_view_)
    {
        LogMsg("[%s] Releasing inspector view", app_name_.c_str());
        inspector_view_ = nullptr;
    }
    if (inspector_texture_id_ != 0)
    {
        LogMsg("[%s] Deleting inspector GL texture", app_name_.c_str());
        glDeleteTextures(1, &inspector_texture_id_);
        inspector_texture_id_ = 0;
    }

    if (main_window_)
    {
        LogMsg("[%s] Destroying X-Plane window", app_name_.c_str());
        XPLMDestroyWindow(main_window_);
        main_window_ = nullptr;
    }
    if (main_view_)
    {
        LogMsg("[%s] Releasing Ultralight view", app_name_.c_str());
        main_view_->set_view_listener(nullptr);
        main_view_->set_load_listener(nullptr);
        main_view_ = nullptr;
    }
    if (texture_id_ != 0)
    {
        LogMsg("[%s] Deleting main GL texture", app_name_.c_str());
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
    if (session_)
    {
        LogMsg("[%s] Releasing session", app_name_.c_str());
        session_ = nullptr;
    }
}

// ── Texture upload ────────────────────────────────────────────────────────────

void UltralightApp::UpdateTexture()
{
    if (!main_view_)
        return;

    Surface *surface = main_view_->surface();
    if (!surface)
        return;

    BitmapSurface *bitmap_surface = static_cast<BitmapSurface *>(surface);
    RefPtr<Bitmap> bitmap = bitmap_surface->bitmap();

    if (!bitmap || bitmap->IsEmpty())
        return;

    if (texture_id_ == 0)
    {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
    else if (!bitmap_surface->dirty_bounds().IsEmpty())
    {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
}

void UltralightApp::ForceRepaint()
{
    if (main_view_)
        main_view_->set_needs_paint(true);
    if (inspector_view_)
        inspector_view_->set_needs_paint(true);
}

void UltralightApp::Reload()
{
    if (main_view_)
    {
        LogMsg("[%s] Reloading view", app_name_.c_str());
        std::string file_url = "file:///apps/" + app_name_ + "/index.html";
        main_view_->LoadURL(file_url.c_str());
    }
}

void UltralightApp::OnWindowResized(int new_width, int new_height)
{
    if (main_view_)
        main_view_->Resize(new_width, new_height);
}

// ── Inspector ─────────────────────────────────────────────────────────────────

void UltralightApp::ShowInspector()
{
    if (!main_view_)
    {
        LogMsg("[%s] Cannot show inspector - main view not initialized", app_name_.c_str());
        return;
    }
    if (inspector_view_ && inspector_window_)
    {
        XPLMSetWindowIsVisible(inspector_window_, 1);
        XPLMBringWindowToFront(inspector_window_);
        LogMsg("[%s] Inspector shown (existing)", app_name_.c_str());
        return;
    }
    if (!inspector_view_)
    {
        LogMsg("[%s] Requesting inspector view creation", app_name_.c_str());
        inspector_pending_ = true;
        main_view_->CreateLocalInspectorView();
    }
    if (inspector_view_ && !inspector_window_)
        CreateInspectorWindow();
}

void UltralightApp::HideInspector()
{
    if (inspector_window_)
    {
        XPLMSetWindowIsVisible(inspector_window_, 0);
        LogMsg("[%s] Inspector hidden", app_name_.c_str());
    }
}

void UltralightApp::ToggleInspector()
{
    if (inspector_window_ && XPLMGetWindowIsVisible(inspector_window_))
        HideInspector();
    else
        ShowInspector();
}

bool UltralightApp::IsInspectorVisible() const
{
    return inspector_window_ && XPLMGetWindowIsVisible(inspector_window_);
}

void UltralightApp::CreateInspectorWindow()
{
    if (!inspector_view_ || inspector_window_)
        return;

    LogMsg("[%s] Creating inspector window", app_name_.c_str());

    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);

    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.left   = winLeft + 150;
    params.right  = winLeft + 150 + inspector_width_;
    params.top    = winTop  - 150;
    params.bottom = winTop  - 150 - inspector_height_;
    params.visible = 1;
    params.refcon  = this;

    params.drawWindowFunc = [](XPLMWindowID /*wnd*/, void *refcon)
    {
        static_cast<UltralightApp *>(refcon)->DrawInspector();
    };
    params.handleMouseClickFunc = [](XPLMWindowID /*wnd*/, int x, int y, int isDown, void *refcon) -> int
    {
        return static_cast<UltralightApp *>(refcon)->OnInspectorMouseClick(x, y, 0, isDown);
    };
    params.handleRightClickFunc = [](XPLMWindowID /*wnd*/, int x, int y, int isDown, void *refcon) -> int
    {
        return static_cast<UltralightApp *>(refcon)->OnInspectorMouseClick(x, y, 1, isDown);
    };
    params.handleMouseWheelFunc = [](XPLMWindowID /*wnd*/, int x, int y, int wheel, int clicks, void *refcon) -> int
    {
        auto *app = static_cast<UltralightApp *>(refcon);
        if (app->inspector_view_)
        {
            ultralight::ScrollEvent evt;
            evt.type    = ultralight::ScrollEvent::kType_ScrollByPixel;
            evt.delta_x = 0;
            evt.delta_y = clicks * 30;
            app->inspector_view_->FireScrollEvent(evt);
            return 1;
        }
        return 0;
    };
    params.handleKeyFunc = [](XPLMWindowID /*wnd*/, char key, XPLMKeyFlags flags, char vk, void *refcon, int losing)
    {
        static_cast<UltralightApp *>(refcon)->OnInspectorKey(key, flags, vk, losing);
    };
    params.handleCursorFunc = [](XPLMWindowID /*wnd*/, int x, int y, void *refcon) -> XPLMCursorStatus
    {
        static_cast<UltralightApp *>(refcon)->OnInspectorMouseMove(x, y);
        return xplm_CursorDefault;
    };

    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    inspector_window_ = XPLMCreateWindowEx(&params);
    std::string title = app_display_name_ + " - Inspector";
    XPLMSetWindowTitle(inspector_window_, title.c_str());
    XPLMSetWindowResizingLimits(inspector_window_, 400, 300, 2000, 2000);

    XPLMDataRef vr_ref = XPLMFindDataRef("sim/graphics/VR/enabled");
    int vr_enabled = vr_ref ? XPLMGetDatai(vr_ref) : 0;
    if (vr_enabled)
        XPLMSetWindowPositioningMode(inspector_window_, xplm_WindowVR, -1);
    else
        XPLMSetWindowPositioningMode(inspector_window_, xplm_WindowPositionFree, -1);

    XPLMSetWindowIsVisible(inspector_window_, 1);
    XPLMBringWindowToFront(inspector_window_);
    LogMsg("[%s] Inspector shown", app_name_.c_str());
}

void UltralightApp::UpdateInspectorTexture()
{
    if (!inspector_view_)
        return;

    Surface *surface = inspector_view_->surface();
    if (!surface)
        return;

    BitmapSurface *bitmap_surface = static_cast<BitmapSurface *>(surface);
    RefPtr<Bitmap> bitmap = bitmap_surface->bitmap();

    if (!bitmap || bitmap->IsEmpty())
        return;

    if (inspector_texture_id_ == 0)
    {
        glGenTextures(1, &inspector_texture_id_);
        glBindTexture(GL_TEXTURE_2D, inspector_texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
    else if (!bitmap_surface->dirty_bounds().IsEmpty())
    {
        glBindTexture(GL_TEXTURE_2D, inspector_texture_id_);
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
}

void UltralightApp::DrawInspector()
{
    if (!inspector_view_ || !inspector_window_)
        return;

    CheckInspectorResize();
    UpdateInspectorTexture();

    if (inspector_texture_id_ == 0)
        return;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);

    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    XPLMBindTexture2d(inspector_texture_id_, 0);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(left,  top);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(left,  bottom);
    glEnd();
}

void UltralightApp::CheckInspectorResize()
{
    if (!inspector_view_ || !inspector_window_)
        return;

    if (++inspector_frame_skip_ < 10)
        return;
    inspector_frame_skip_ = 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);

    int new_width  = right - left;
    int new_height = top   - bottom;

    if (new_width != inspector_width_ || new_height != inspector_height_)
    {
        LogMsg("[%s] Inspector resized: %dx%d -> %dx%d",
               app_name_.c_str(),
               inspector_width_, inspector_height_,
               new_width, new_height);

        inspector_width_  = new_width;
        inspector_height_ = new_height;
        inspector_view_->Resize(inspector_width_, inspector_height_);

        if (inspector_texture_id_ != 0)
        {
            glDeleteTextures(1, &inspector_texture_id_);
            inspector_texture_id_ = 0;
        }
    }
}

// ── Input routing ─────────────────────────────────────────────────────────────

int UltralightApp::OnMouseClick(int x, int y, int button, int mouseStatus)
{
    if (!main_view_ || !main_window_)
        return 0;

    bool isDown = (mouseStatus == xplm_MouseDown);
    bool isUp   = (mouseStatus == xplm_MouseUp);
    bool isDrag = (mouseStatus == xplm_MouseDrag);

    if (isDown)
    {
        XPLMTakeKeyboardFocus(main_window_);
        main_view_->Focus();
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    // X-Plane: origin bottom-left. Ultralight: origin top-left.
    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    ultralight::MouseEvent evt;
    evt.x      = view_x;
    evt.y      = view_y;
    evt.button = (button == 0) ? ultralight::MouseEvent::kButton_Left
                               : ultralight::MouseEvent::kButton_Right;

    if (isDown)
    {
        evt.type = ultralight::MouseEvent::kType_MouseDown;
        main_view_->FireMouseEvent(evt);
    }
    else if (isUp)
    {
        evt.type = ultralight::MouseEvent::kType_MouseUp;
        main_view_->FireMouseEvent(evt);
    }
    else if (isDrag)
    {
        evt.type = ultralight::MouseEvent::kType_MouseMoved;
        main_view_->FireMouseEvent(evt);
    }

    return 1;
}

int UltralightApp::OnMouseMove(int x, int y)
{
    if (!main_view_ || !main_window_)
        return 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    ultralight::MouseEvent evt;
    evt.type   = ultralight::MouseEvent::kType_MouseMoved;
    evt.x      = view_x;
    evt.y      = view_y;
    evt.button = ultralight::MouseEvent::kButton_None;

    main_view_->FireMouseEvent(evt);
    return 1;
}

int UltralightApp::OnMouseWheel(int /*x*/, int /*y*/, int /*wheel*/, int clicks)
{
    if (!main_view_)
        return 0;

    ultralight::ScrollEvent evt;
    evt.type    = ultralight::ScrollEvent::kType_ScrollByPixel;
    evt.delta_x = 0;
    evt.delta_y = clicks * 30;
    main_view_->FireScrollEvent(evt);
    return 1;
}

void UltralightApp::OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus)
{
    if (!main_view_)
        return;

    if (losingFocus)
    {
        main_view_->Unfocus();
        return;
    }

    bool isDown = (flags & xplm_DownFlag) != 0;
    ultralight::KeyEvent evt;

    if (isDown)
    {
        if (key >= 32 && key < 127)
        {
            evt.type             = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code  = virtualKey;
            evt.modifiers        = 0;
            if (flags & xplm_ShiftFlag)     evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag)   evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            main_view_->FireKeyEvent(evt);

            evt.type            = ultralight::KeyEvent::kType_Char;
            evt.text            = ultralight::String8(&key, 1);
            evt.unmodified_text = evt.text;
            main_view_->FireKeyEvent(evt);
        }
        else
        {
            evt.type             = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code  = virtualKey;
            evt.modifiers        = 0;
            if (flags & xplm_ShiftFlag)     evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag)   evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            main_view_->FireKeyEvent(evt);
        }
    }
    else
    {
        evt.type             = ultralight::KeyEvent::kType_KeyUp;
        evt.virtual_key_code = virtualKey;
        evt.native_key_code  = virtualKey;
        evt.modifiers        = 0;
        main_view_->FireKeyEvent(evt);
    }
}

// Inspector input

int UltralightApp::OnInspectorMouseClick(int x, int y, int button, int mouseStatus)
{
    if (!inspector_view_ || !inspector_window_)
        return 0;

    bool isDown = (mouseStatus == xplm_MouseDown);
    bool isUp   = (mouseStatus == xplm_MouseUp);
    bool isDrag = (mouseStatus == xplm_MouseDrag);

    if (isDown)
    {
        XPLMTakeKeyboardFocus(inspector_window_);
        inspector_view_->Focus();
    }

    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);

    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    ultralight::MouseEvent evt;
    evt.x      = view_x;
    evt.y      = view_y;
    evt.button = (button == 0) ? ultralight::MouseEvent::kButton_Left
                               : ultralight::MouseEvent::kButton_Right;

    if (isDown)
    {
        evt.type = ultralight::MouseEvent::kType_MouseDown;
        inspector_view_->FireMouseEvent(evt);
    }
    else if (isUp)
    {
        evt.type = ultralight::MouseEvent::kType_MouseUp;
        inspector_view_->FireMouseEvent(evt);
    }
    else if (isDrag)
    {
        evt.type = ultralight::MouseEvent::kType_MouseMoved;
        inspector_view_->FireMouseEvent(evt);
    }
    return 1;
}

int UltralightApp::OnInspectorMouseMove(int x, int y)
{
    if (!inspector_view_ || !inspector_window_)
        return 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);

    int view_x = x - left;
    int view_y = (top - bottom) - (y - bottom);

    ultralight::MouseEvent evt;
    evt.type   = ultralight::MouseEvent::kType_MouseMoved;
    evt.x      = view_x;
    evt.y      = view_y;
    evt.button = ultralight::MouseEvent::kButton_None;
    inspector_view_->FireMouseEvent(evt);
    return 1;
}

void UltralightApp::OnInspectorKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus)
{
    if (!inspector_view_)
        return;

    if (losingFocus)
    {
        inspector_view_->Unfocus();
        return;
    }

    bool isDown = (flags & xplm_DownFlag) != 0;
    ultralight::KeyEvent evt;

    if (isDown)
    {
        if (key >= 32 && key < 127)
        {
            evt.type             = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code  = virtualKey;
            evt.modifiers        = 0;
            if (flags & xplm_ShiftFlag)     evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag)   evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            inspector_view_->FireKeyEvent(evt);

            evt.type            = ultralight::KeyEvent::kType_Char;
            evt.text            = ultralight::String8(&key, 1);
            evt.unmodified_text = evt.text;
            inspector_view_->FireKeyEvent(evt);
        }
        else
        {
            evt.type             = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code  = virtualKey;
            evt.modifiers        = 0;
            if (flags & xplm_ShiftFlag)     evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag)   evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            inspector_view_->FireKeyEvent(evt);
        }
    }
    else
    {
        evt.type             = ultralight::KeyEvent::kType_KeyUp;
        evt.virtual_key_code = virtualKey;
        evt.native_key_code  = virtualKey;
        evt.modifiers        = 0;
        inspector_view_->FireKeyEvent(evt);
    }
}

// ── Ultralight LoadListener ───────────────────────────────────────────────────

void UltralightApp::OnAddConsoleMessage(View *caller, const ConsoleMessage &msg)
{
    LogMsg("[%s] Console: %s (line %d, source: %s)",
           app_name_.c_str(),
           msg.message().utf8().data(),
           msg.line_number(),
           msg.source_id().utf8().data());
}

void UltralightApp::OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] BeginLoading: %s (main_frame=%d)", app_name_.c_str(), url.utf8().data(), is_main_frame);
}

void UltralightApp::OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] FinishLoading: %s (main_frame=%d)", app_name_.c_str(), url.utf8().data(), is_main_frame);
}

void UltralightApp::OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame,
                                   const String &url, const String &description,
                                   const String &error_domain, int error_code)
{
    LogMsg("[%s] FAILED Loading: %s - Error: %s (domain: %s, code: %d)",
           app_name_.c_str(),
           url.utf8().data(),
           description.utf8().data(),
           error_domain.utf8().data(),
           error_code);
}

void UltralightApp::OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] DOMReady: %s (main_frame=%d)", app_name_.c_str(), url.utf8().data(), is_main_frame);

    if (is_main_frame && main_view_)
    {
        LogMsg("[%s] Binding XPlane API to JavaScript context", app_name_.c_str());
        JSBindings::BindToView(main_view_, app_name_, app_display_name_, app_dir_);
    }
}

// ── Ultralight ViewListener ───────────────────────────────────────────────────

RefPtr<View> UltralightApp::OnCreateInspectorView(View *caller, bool is_local,
                                                   const String &inspected_url)
{
    LogMsg("[%s] OnCreateInspectorView (is_local=%d, url=%s)",
           app_name_.c_str(), is_local, inspected_url.utf8().data());

    if (!renderer_)
    {
        LogMsg("[%s] Cannot create inspector view - renderer not available", app_name_.c_str());
        return nullptr;
    }

    inspector_view_ = renderer_->CreateView(inspector_width_, inspector_height_, ViewConfig(), nullptr);
    if (!inspector_view_)
    {
        LogMsg("[%s] Failed to create inspector view", app_name_.c_str());
        return nullptr;
    }

    LogMsg("[%s] Inspector view created successfully", app_name_.c_str());

    if (inspector_pending_)
    {
        inspector_pending_ = false;
        CreateInspectorWindow();
    }

    return inspector_view_;
}
