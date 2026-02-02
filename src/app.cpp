#include "app.h"
#include "js_bindings.h"

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
    Destroy();
}

void App::Destroy()
{
    // Destroy inspector first
    if (inspector_window_)
    {
        LogMsg("[%s] Destroying inspector window", app_name.c_str());
        XPLMDestroyWindow(inspector_window_);
        inspector_window_ = nullptr;
    }
    
    if (inspector_view_)
    {
        LogMsg("[%s] Releasing inspector view", app_name.c_str());
        inspector_view_ = nullptr;
    }
    
    if (inspector_texture_id_ != 0)
    {
        LogMsg("[%s] Deleting inspector OpenGL texture", app_name.c_str());
        glDeleteTextures(1, &inspector_texture_id_);
        inspector_texture_id_ = 0;
    }
    
    // Destroy X-Plane window first
    if (main_window_)
    {
        LogMsg("[%s] Destroying X-Plane window", app_name.c_str());
        XPLMDestroyWindow(main_window_);
        main_window_ = nullptr;
    }
    
    // Release the Ultralight view
    if (main_view_)
    {
        LogMsg("[%s] Releasing Ultralight view", app_name.c_str());
        main_view_->set_view_listener(nullptr);
        main_view_->set_load_listener(nullptr);
        main_view_ = nullptr;  // RefPtr will release the view
    }
    
    // Delete OpenGL texture
    if (texture_id_ != 0)
    {
        LogMsg("[%s] Deleting OpenGL texture", app_name.c_str());
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
}

void App::ForceRepaint()
{
    if (main_view_)
    {
        main_view_->set_needs_paint(true);
    }
    if (inspector_view_)
    {
        inspector_view_->set_needs_paint(true);
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

    // Store renderer reference for creating inspector view later
    renderer_ = renderer;

    // create a view for this app with actual dimensions
    view_width_ = 800;
    view_height_ = 600;
    main_view_ = renderer->CreateView(view_width_, view_height_, ViewConfig(), nullptr);
    main_view_->set_view_listener(this);
    main_view_->set_load_listener(this);

    // Note: JS bindings are set up in OnDOMReady after the page loads

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
    
    // Check if VR is enabled and set appropriate window mode
    XPLMDataRef vr_enabled_ref = XPLMFindDataRef("sim/graphics/VR/enabled");
    int vr_enabled = vr_enabled_ref ? XPLMGetDatai(vr_enabled_ref) : 0;
    if (vr_enabled) {
        XPLMSetWindowPositioningMode(main_window_, xplm_WindowVR, -1);
    } else {
        XPLMSetWindowPositioningMode(main_window_, xplm_WindowPositionFree, -1);
    }
    
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

void App::Reload()
{
    if (main_view_)
    {
        LogMsg("[%s] Reloading view", app_name.c_str());
        // Reload the current URL (index.html) to refresh HTML/JS
        std::string relative_path = "apps/" + app_name + "/index.html";
        std::string file_url = "file:///" + relative_path;
        main_view_->LoadURL(file_url.c_str());
    }
}

// =========================================================================
// Inspector Support
// =========================================================================

void App::ShowInspector()
{
    if (!main_view_)
    {
        LogMsg("[%s] Cannot show inspector - main view not initialized", app_name.c_str());
        return;
    }
    
    // If inspector view already exists and window is created, just show it
    if (inspector_view_ && inspector_window_)
    {
        XPLMSetWindowIsVisible(inspector_window_, 1);
        XPLMBringWindowToFront(inspector_window_);
        LogMsg("[%s] Inspector shown (existing)", app_name.c_str());
        return;
    }
    
    // Request inspector view creation - this will trigger OnCreateInspectorView callback
    if (!inspector_view_)
    {
        LogMsg("[%s] Requesting inspector view creation", app_name.c_str());
        inspector_pending_ = true;
        main_view_->CreateLocalInspectorView();
        // The OnCreateInspectorView callback will be called and set inspector_view_
        // Then we can create the window
    }
    
    // If inspector view was just created (in the callback), create the window
    if (inspector_view_ && !inspector_window_)
    {
        CreateInspectorWindow();
    }
}

void App::CreateInspectorWindow()
{
    if (!inspector_view_ || inspector_window_)
        return;
        
    LogMsg("[%s] Creating inspector window", app_name.c_str());
    
    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);
    
    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.left = winLeft + 150;
    params.right = winLeft + 150 + inspector_width_;
    params.top = winTop - 150;
    params.bottom = winTop - 150 - inspector_height_;
    params.visible = 1;
    params.refcon = this;
    params.drawWindowFunc = [](XPLMWindowID wnd, void *refcon)
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->DrawInspector();
        }
    };
    params.handleMouseClickFunc = [](XPLMWindowID wnd, int x, int y, int isDown, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            return app->OnInspectorMouseClick(x, y, 0, isDown);
        }
        return 0;
    };
    params.handleRightClickFunc = [](XPLMWindowID wnd, int x, int y, int isDown, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            return app->OnInspectorMouseClick(x, y, 1, isDown);
        }
        return 0;
    };
    params.handleMouseWheelFunc = [](XPLMWindowID wnd, int x, int y, int wheel, int clicks, void *refcon) -> int
    {
        App *app = static_cast<App *>(refcon);
        if (app && app->inspector_view_)
        {
            ultralight::ScrollEvent evt;
            evt.type = ultralight::ScrollEvent::kType_ScrollByPixel;
            evt.delta_x = 0;
            evt.delta_y = clicks * 30;
            app->inspector_view_->FireScrollEvent(evt);
            return 1;
        }
        return 0;
    };
    params.handleKeyFunc = [](XPLMWindowID wnd, char key, XPLMKeyFlags flags, char virtualKey, void *refcon, int losingFocus)
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->OnInspectorKey(key, flags, virtualKey, losingFocus);
        }
    };
    params.handleCursorFunc = [](XPLMWindowID wnd, int x, int y, void *refcon) -> XPLMCursorStatus
    {
        App *app = static_cast<App *>(refcon);
        if (app)
        {
            app->OnInspectorMouseMove(x, y);
        }
        return xplm_CursorDefault;
    };
    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    
    inspector_window_ = XPLMCreateWindowEx(&params);
    std::string title = app_name + " - Inspector";
    XPLMSetWindowTitle(inspector_window_, title.c_str());
    XPLMSetWindowResizingLimits(inspector_window_, 400, 300, 2000, 2000);
    
    // Check if VR is enabled and set appropriate window mode
    XPLMDataRef vr_enabled_ref = XPLMFindDataRef("sim/graphics/VR/enabled");
    int vr_enabled = vr_enabled_ref ? XPLMGetDatai(vr_enabled_ref) : 0;
    if (vr_enabled) {
        XPLMSetWindowPositioningMode(inspector_window_, xplm_WindowVR, -1);
    } else {
        XPLMSetWindowPositioningMode(inspector_window_, xplm_WindowPositionFree, -1);
    }
    
    XPLMSetWindowIsVisible(inspector_window_, 1);
    XPLMBringWindowToFront(inspector_window_);
    LogMsg("[%s] Inspector shown", app_name.c_str());
}

void App::HideInspector()
{
    if (inspector_window_)
    {
        XPLMSetWindowIsVisible(inspector_window_, 0);
        LogMsg("[%s] Inspector hidden", app_name.c_str());
    }
}

void App::ToggleInspector()
{
    if (inspector_window_ && XPLMGetWindowIsVisible(inspector_window_))
    {
        HideInspector();
    }
    else
    {
        ShowInspector();
    }
}

bool App::IsInspectorVisible() const
{
    return inspector_window_ && XPLMGetWindowIsVisible(inspector_window_);
}

void App::UpdateInspectorTexture()
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
    
    // Create texture if needed
    if (inspector_texture_id_ == 0)
    {
        glGenTextures(1, &inspector_texture_id_);
        glBindTexture(GL_TEXTURE_2D, inspector_texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
    
    // Upload bitmap to texture if dirty
    if (bitmap_surface->dirty_bounds().IsEmpty() == false)
    {
        glBindTexture(GL_TEXTURE_2D, inspector_texture_id_);
        void *pixels = bitmap->LockPixels();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(),
                     0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        bitmap->UnlockPixels();
        bitmap_surface->ClearDirtyBounds();
    }
}

void App::DrawInspector()
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

void App::CheckInspectorResize()
{
    if (!inspector_view_ || !inspector_window_)
        return;
    
    static int frame_skip = 0;
    if (++frame_skip < 10)
        return;
    frame_skip = 0;
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);
    
    int new_width = right - left;
    int new_height = top - bottom;
    
    if (new_width != inspector_width_ || new_height != inspector_height_)
    {
        LogMsg("[%s] Inspector resized: %dx%d -> %dx%d",
               app_name.c_str(), inspector_width_, inspector_height_, new_width, new_height);
        
        inspector_width_ = new_width;
        inspector_height_ = new_height;
        
        inspector_view_->Resize(inspector_width_, inspector_height_);
        
        if (inspector_texture_id_ != 0)
        {
            glDeleteTextures(1, &inspector_texture_id_);
            inspector_texture_id_ = 0;
        }
    }
}

int App::OnInspectorMouseClick(int x, int y, int button, int mouseStatus)
{
    if (!inspector_view_ || !inspector_window_)
        return 0;
    
    bool isDown = (mouseStatus == xplm_MouseDown);
    bool isUp = (mouseStatus == xplm_MouseUp);
    bool isDrag = (mouseStatus == xplm_MouseDrag);
    
    if (isDown)
    {
        XPLMTakeKeyboardFocus(inspector_window_);
        inspector_view_->Focus();
    }
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);
    
    int view_x = x - left;
    int view_y = y - bottom;
    view_y = (top - bottom) - view_y;
    
    ultralight::MouseEvent evt;
    evt.x = view_x;
    evt.y = view_y;
    evt.button = (button == 0) ? ultralight::MouseEvent::kButton_Left : ultralight::MouseEvent::kButton_Right;
    
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

int App::OnInspectorMouseMove(int x, int y)
{
    if (!inspector_view_ || !inspector_window_)
        return 0;
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(inspector_window_, &left, &top, &right, &bottom);
    
    int view_x = x - left;
    int view_y = y - bottom;
    view_y = (top - bottom) - view_y;
    
    ultralight::MouseEvent evt;
    evt.type = ultralight::MouseEvent::kType_MouseMoved;
    evt.x = view_x;
    evt.y = view_y;
    evt.button = ultralight::MouseEvent::kButton_None;
    
    inspector_view_->FireMouseEvent(evt);
    return 1;
}

void App::OnInspectorKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus)
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
            evt.type = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code = virtualKey;
            evt.modifiers = 0;
            if (flags & xplm_ShiftFlag) evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag) evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            inspector_view_->FireKeyEvent(evt);
            
            evt.type = ultralight::KeyEvent::kType_Char;
            evt.text = ultralight::String8(&key, 1);
            evt.unmodified_text = evt.text;
            inspector_view_->FireKeyEvent(evt);
        }
        else
        {
            evt.type = ultralight::KeyEvent::kType_RawKeyDown;
            evt.virtual_key_code = virtualKey;
            evt.native_key_code = virtualKey;
            evt.modifiers = 0;
            if (flags & xplm_ShiftFlag) evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
            if (flags & xplm_OptionAltFlag) evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
            if (flags & xplm_ControlFlag) evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
            inspector_view_->FireKeyEvent(evt);
        }
    }
    else
    {
        evt.type = ultralight::KeyEvent::kType_KeyUp;
        evt.virtual_key_code = virtualKey;
        evt.native_key_code = virtualKey;
        evt.modifiers = 0;
        inspector_view_->FireKeyEvent(evt);
    }
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
    
    // Bind X-Plane API to JavaScript context now that the page is ready
    if (is_main_frame && main_view_) {
        LogMsg("[%s] Binding XPlane API to JavaScript context", app_name.c_str());
        JSBindings::BindToView(main_view_);
    }
}

RefPtr<View> App::OnCreateInspectorView(View *caller, bool is_local, const String &inspected_url)
{
    LogMsg("[%s] OnCreateInspectorView called (is_local=%d, url=%s)", 
           app_name.c_str(), is_local, inspected_url.utf8().data());
    
    if (!renderer_)
    {
        LogMsg("[%s] Cannot create inspector view - renderer not available", app_name.c_str());
        return nullptr;
    }
    
    // Create inspector view with the same dimensions
    inspector_view_ = renderer_->CreateView(inspector_width_, inspector_height_, ViewConfig(), nullptr);
    
    if (!inspector_view_)
    {
        LogMsg("[%s] Failed to create inspector view", app_name.c_str());
        return nullptr;
    }
    
    LogMsg("[%s] Inspector view created successfully", app_name.c_str());
    
    // If the inspector was pending (requested via ShowInspector), create the window
    if (inspector_pending_)
    {
        inspector_pending_ = false;
        CreateInspectorWindow();
    }
    
    return inspector_view_;
}