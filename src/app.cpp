#include "app.h"

#include <fstream>
#include <cstring>
#include <vector>

// Set to 1 to enable debug logging and screenshot saving
#define DEBUG_DRAW 0

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

App::App() : app_name(""), app_dir("") {}

App::App(const std::string &name, const std::string &dir)
    : app_name(name), app_dir(dir)
{
    LogMsg("App created: %s, dir: %s", app_name.c_str(), app_dir.c_str());
}

App::~App()
{
    if (texture_id_ != 0)
    {
        glDeleteTextures(1, &texture_id_);
    }
}

void App::ForceRepaint()
{
    if (main_view_)
    {
        main_view_->set_needs_paint(true);
    }
}

void App::UpdateTexture()
{
    if (!main_view_)
        return;

    // Get the rendered surface from Ultralight
    Surface *surface = main_view_->surface();
    if (!surface)
        return;

    BitmapSurface *bitmap_surface = static_cast<BitmapSurface *>(surface);
    RefPtr<Bitmap> bitmap = bitmap_surface->bitmap();

    if (!bitmap || bitmap->IsEmpty())
        return;

    // Create texture if needed
    if (texture_id_ == 0)
    {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Force initial texture upload
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
        texture_needs_init_ = false;
    }

    // Upload bitmap to texture if dirty
    if (bitmap_surface->dirty_bounds().IsEmpty() == false)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
}

void App::Draw()
{
    if (!main_view_ || !main_window_)
        return;

    // Check if window was resized
    CheckResize();

    // Ensure texture is up to date
    UpdateTexture();

    if (texture_id_ == 0)
        return;

    // Get window geometry
    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    XPLMSetGraphicsState(
        0, // No fog
        1, // One texture unit
        0, // No lighting
        0, // No alpha testing
        1, // Alpha blending
        0, // No depth read
        0  // No depth write
    );

    XPLMBindTexture2d(texture_id_, 0);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    // Note: Ultralight renders top-down, OpenGL is bottom-up
    // Flip V coordinates: use 0 at top, 1 at bottom
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(left, top);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(right, top);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(right, bottom);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(left, bottom);
    glEnd();
}

void App::Initialize(RefPtr<Renderer> renderer)
{
    LogMsg("Initializing app: %s", app_name.c_str());

    // create a view for this app with actual dimensions
    view_width_ = 800;
    view_height_ = 600;
    main_view_ = renderer->CreateView(view_width_, view_height_, ViewConfig(), nullptr);
    main_view_->set_view_listener(this);
    main_view_->set_load_listener(this);

    // Load index.html using Ultralight's FileSystem (configured with plugin_dir as base)
    // Path is relative to plugin_dir, e.g., "apps/app_manager/index.html"
    std::string relative_path = "apps/" + app_name + "/index.html";
    std::string file_url = "file:///" + relative_path;
    LogMsg("Loading URL: %s", file_url.c_str());
    main_view_->LoadURL(file_url.c_str());

    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);

    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.left = winLeft + 100;
    params.right = winLeft + 100 + view_width_;
    params.top = winTop - 100;
    params.bottom = winTop - 100 - view_height_;
    params.visible = 1;
    params.refcon = this;
    params.drawWindowFunc = [](XPLMWindowID wnd, void *refcon)
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->Draw();
        }
    };
    params.handleMouseClickFunc = [](XPLMWindowID wnd, int x, int y, int isDown, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            return app->OnMouseClick(x, y, 0, isDown);
        }
        return 0;
    };
    params.handleRightClickFunc = [](XPLMWindowID wnd, int x, int y, int isDown, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            return app->OnMouseClick(x, y, 1, isDown);
        }
        return 0;
    };
    params.handleMouseWheelFunc = [](XPLMWindowID wnd, int x, int y, int wheel, int clicks, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app && app->main_view_)
        {
            // Get window geometry to convert coordinates
            int left, top, right, bottom;
            XPLMGetWindowGeometry(wnd, &left, &top, &right, &bottom);
            
            ultralight::ScrollEvent evt;
            evt.type = ultralight::ScrollEvent::kType_ScrollByPixel;
            evt.delta_x = 0;
            evt.delta_y = clicks * 30;  // Scroll amount
            app->main_view_->FireScrollEvent(evt);
            return 1;
        }
        return 0;
    };
    params.handleKeyFunc = [](XPLMWindowID wnd, char key, XPLMKeyFlags flags, char virtualKey, void *refcon, int losingFocus)
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->OnKey(key, flags, virtualKey, losingFocus);
        }
    };
    params.handleCursorFunc = [](XPLMWindowID wnd, int x, int y, void *refcon) -> XPLMCursorStatus
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->OnMouseMove(x, y);
        }
        return xplm_CursorDefault;
    };
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    main_window_ = XPLMCreateWindowEx(&params);
    XPLMSetWindowTitle(main_window_, app_name.c_str());
    XPLMSetWindowResizingLimits(main_window_, 200, 200, 2000, 2000);  // Allow resizing
    XPLMSetWindowIsVisible(main_window_, 0);  // Hidden by default - use menu to show
}

void App::Show()
{
    if (main_window_)
    {
        XPLMSetWindowIsVisible(main_window_, 1);
        XPLMBringWindowToFront(main_window_);
    }
}

void App::Hide()
{
    if (main_window_)
    {
        XPLMSetWindowIsVisible(main_window_, 0);
    }
}

void App::Toggle()
{
    if (main_window_)
    {
        if (XPLMGetWindowIsVisible(main_window_))
        {
            Hide();
        }
        else
        {
            Show();
        }
    }
}

bool App::IsVisible() const
{
    return main_window_ && XPLMGetWindowIsVisible(main_window_);
}

void App::CheckResize()
{
    if (!main_view_ || !main_window_)
        return;

    // Only check every 10 frames to reduce overhead
    static int frame_skip = 0;
    if (++frame_skip < 10)
        return;
    frame_skip = 0;

    // Get current window dimensions
    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);
    
    int new_width = right - left;
    int new_height = top - bottom;
    
    // Check if size changed
    if (new_width != view_width_ || new_height != view_height_)
    {
        LogMsg("[%s] Window resized: %dx%d -> %dx%d", 
               app_name.c_str(), view_width_, view_height_, new_width, new_height);
        
        view_width_ = new_width;
        view_height_ = new_height;
        
        // Resize the Ultralight view
        main_view_->Resize(view_width_, view_height_);
        
        // Delete old texture so a new one is created with correct size
        if (texture_id_ != 0)
        {
            glDeleteTextures(1, &texture_id_);
            texture_id_ = 0;
        }
    }
}

int App::OnMouseClick(int x, int y, int button, int mouseStatus)
{
    if (!main_view_ || !main_window_)
    {
        return 0;
    }

    // mouseStatus is XPLMMouseStatus: 1=Down, 2=Drag, 3=Up
    bool isDown = (mouseStatus == xplm_MouseDown);
    bool isUp = (mouseStatus == xplm_MouseUp);
    bool isDrag = (mouseStatus == xplm_MouseDrag);

    // Focus the view on click so it receives keyboard input
    if (isDown)
    {
        // Take keyboard focus from X-Plane so we receive key events
        XPLMTakeKeyboardFocus(main_window_);
        // Also focus the Ultralight view
        main_view_->Focus();
    }

    // Get window geometry to convert coordinates
    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    // Convert X-Plane coordinates to view coordinates
    // X-Plane: origin bottom-left, Y increases upward
    // Ultralight: origin top-left, Y increases downward
    int view_x = x - left;
    int view_y = y - bottom;  // Convert to window-relative, Y from bottom
    view_y = (top - bottom) - view_y;  // Flip to top-down for Ultralight

    ultralight::MouseEvent evt;
    evt.x = view_x;
    evt.y = view_y;
    evt.button = (button == 0) ? ultralight::MouseEvent::kButton_Left : ultralight::MouseEvent::kButton_Right;

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
        // For drag, send mouse move event (button is already tracked)
        evt.type = ultralight::MouseEvent::kType_MouseMoved;
        main_view_->FireMouseEvent(evt);
    }

    return 1;
}

int App::OnMouseMove(int x, int y)
{
    if (!main_view_ || !main_window_)
        return 0;

    // Get window geometry to convert coordinates
    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    // Convert X-Plane coordinates to view coordinates (same as OnMouseClick)
    int view_x = x - left;
    int view_y = y - bottom;
    view_y = (top - bottom) - view_y;  // Flip to top-down for Ultralight

    ultralight::MouseEvent evt;
    evt.type = ultralight::MouseEvent::kType_MouseMoved;
    evt.x = view_x;
    evt.y = view_y;
    evt.button = ultralight::MouseEvent::kButton_None;

    main_view_->FireMouseEvent(evt);
    return 1;
}

void App::OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus)
{
    if (!main_view_)
        return;
    
    if (losingFocus)
    {
        main_view_->Unfocus();
        return;
    }

    // Determine if this is a key down or key up
    bool isDown = (flags & xplm_DownFlag) != 0;

    ultralight::KeyEvent evt;
    
    if (isDown)
    {
        // For printable characters, send both KeyDown and Char events
        if (key >= 32 && key < 127)
        {
            // First send KeyDown
            evt.type = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code = virtualKey;
            evt.modifiers = 0;
            if (flags & xplm_ShiftFlag) evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag) evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            main_view_->FireKeyEvent(evt);
            
            // Then send Char event for text input
            evt.type = ultralight::KeyEvent::kType_Char;
            evt.text = ultralight::String8(&key, 1);
            evt.unmodified_text = evt.text;
            main_view_->FireKeyEvent(evt);
        }
        else
        {
            // Non-printable key (backspace, enter, arrows, etc.)
            evt.type = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code = virtualKey;
            evt.modifiers = 0;
            if (flags & xplm_ShiftFlag) evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag) evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            main_view_->FireKeyEvent(evt);
        }
    }
    else
    {
        // Key up
        evt.type = ultralight::KeyEvent::kType_KeyUp;
        evt.virtual_key_code = virtualKey;
        evt.native_key_code = virtualKey;
        evt.modifiers = 0;
        main_view_->FireKeyEvent(evt);
    }
}

void App::OnAddConsoleMessage(View *caller, const ConsoleMessage &msg)
{
    LogMsg("[%s] Console: %s (line %d, source: %s)",
           app_name.c_str(),
           msg.message().utf8().data(),
           msg.line_number(),
           msg.source_id().utf8().data());
}

void App::OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] BeginLoading: %s (main_frame=%d)", app_name.c_str(), url.utf8().data(), is_main_frame);
}

void App::OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] FinishLoading: %s (main_frame=%d)", app_name.c_str(), url.utf8().data(), is_main_frame);
}

void App::OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url, const String &description, const String &error_domain, int error_code)
{
    LogMsg("[%s] FAILED Loading: %s - Error: %s (domain: %s, code: %d)",
           app_name.c_str(),
           url.utf8().data(),
           description.utf8().data(),
           error_domain.utf8().data(),
           error_code);
}

void App::OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
    LogMsg("[%s] DOMReady: %s (main_frame=%d)", app_name.c_str(), url.utf8().data(), is_main_frame);
}