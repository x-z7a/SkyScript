#include "app.h"
#include "js_bindings.h"
#include "manager.h"

#include <fstream>
#include <cstring>
#include <vector>
#include <filesystem>

// Set to 1 to enable debug logging and screenshot saving
#define DEBUG_DRAW 0

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

App::App() : app_name(""), app_dir("") {}

App::App(const std::string &name, const std::string &dir)
    : app_name(name), app_dir(dir), app_type(AppType::Local)
{
    LogMsg("App created (local): %s, dir: %s", app_name.c_str(), app_dir.c_str());
}

App::App(const std::string &name, const std::string &url, AppType type)
    : app_name(name), app_url(url), app_type(type)
{
    LogMsg("App created (URL): %s, url: %s", app_name.c_str(), app_url.c_str());
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
    
    // Release session
    if (session_)
    {
        LogMsg("[%s] Releasing session", app_name.c_str());
        session_ = nullptr;
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
    LogMsg("Initializing app: %s (type=%s)", app_name.c_str(), 
           app_type == AppType::Local ? "local" : "url");

    // Store renderer reference for creating inspector view later
    renderer_ = renderer;

    // Create the app's cache folder for session storage
    std::string app_cache_dir = Manager::instance().getOutputDir() + "/cache/" + app_name;
    std::filesystem::create_directories(app_cache_dir);

    // Create a persistent session for this app (isolated cookies, localStorage, indexedDB, etc.)
    session_ = renderer->CreateSession(true, app_name.c_str());
    LogMsg("[%s] Created session: %s (persistent=%d, path=%s)", 
           app_name.c_str(), 
           session_->name().utf8().data(),
           session_->is_persistent(),
           session_->disk_path().utf8().data());

    // create a view for this app with actual dimensions
    view_width_ = 800;
    view_height_ = 600;
    main_view_ = renderer->CreateView(view_width_, view_height_, ViewConfig(), session_);
    main_view_->set_view_listener(this);
    main_view_->set_load_listener(this);

    // Note: JS bindings are set up in OnDOMReady after the page loads

    // Load content based on app type
    std::string load_url;
    if (app_type == AppType::Local) {
        // Load index.html using Ultralight's FileSystem (configured with plugin_dir as base)
        // Path is relative to plugin_dir, e.g., "apps/app_manager/index.html"
        std::string relative_path = "apps/" + app_name + "/index.html";
        load_url = "file:///" + relative_path;
    } else {
        // URL app - load directly from the URL
        load_url = app_url;
    }
    LogMsg("Loading URL: %s", load_url.c_str());
    main_view_->LoadURL(load_url.c_str());

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
        
        std::string load_url;
        if (app_type == AppType::Local) {
            // Reload from local file
            std::string relative_path = "apps/" + app_name + "/index.html";
            load_url = "file:///" + relative_path;
        } else {
            // Reload from URL
            load_url = app_url;
        }
        main_view_->LoadURL(load_url.c_str());
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
        
        // Inject WebCrypto polyfill for external HTTPS content (e.g., Navigraph Charts)
        // This patches the SubtleCrypto API to allow crypto operations in embedded contexts
        std::string webcrypto_patch = R"(
(function() {
    // Simple PRNG based on Math.random (not cryptographically secure, but works for most use cases)
    var getRandomValues = function(array) {
        for (var i = 0; i < array.length; i++) {
            if (array instanceof Uint8Array) {
                array[i] = Math.floor(Math.random() * 256);
            } else if (array instanceof Uint16Array) {
                array[i] = Math.floor(Math.random() * 65536);
            } else if (array instanceof Uint32Array) {
                array[i] = Math.floor(Math.random() * 4294967296);
            } else {
                array[i] = Math.floor(Math.random() * 256);
            }
        }
        return array;
    };
    
    var randomUUID = function() {
        // Generate UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        var bytes = new Uint8Array(16);
        getRandomValues(bytes);
        bytes[6] = (bytes[6] & 0x0f) | 0x40; // Version 4
        bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 1
        
        var hex = Array.from(bytes).map(function(b) {
            return b.toString(16).padStart(2, '0');
        }).join('');
        
        return hex.slice(0, 8) + '-' + hex.slice(8, 12) + '-' + hex.slice(12, 16) + '-' + hex.slice(16, 20) + '-' + hex.slice(20);
    };
    
    // =========================================================================
    // SHA-256 Implementation (pure JavaScript)
    // =========================================================================
    var sha256 = function(message) {
        var K = [
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        ];
        
        var H = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
        
        var rightRotate = function(value, amount) {
            return (value >>> amount) | (value << (32 - amount));
        };
        
        // Convert to bytes if string
        var bytes;
        if (typeof message === 'string') {
            bytes = new Uint8Array(new TextEncoder().encode(message));
        } else if (message instanceof ArrayBuffer) {
            bytes = new Uint8Array(message);
        } else {
            bytes = new Uint8Array(message);
        }
        
        // Pre-processing: adding padding bits
        var msgLen = bytes.length;
        var bitLen = msgLen * 8;
        var padLen = ((msgLen + 8) % 64 < 56) ? (56 - (msgLen + 8) % 64) : (120 - (msgLen + 8) % 64);
        padLen += 8;
        
        var padded = new Uint8Array(msgLen + padLen + 8);
        padded.set(bytes);
        padded[msgLen] = 0x80;
        
        // Append length in bits as 64-bit big-endian
        var lenPos = padded.length - 8;
        for (var i = 0; i < 8; i++) {
            padded[lenPos + i] = (i < 4) ? 0 : ((bitLen >>> ((7 - i) * 8)) & 0xff);
        }
        
        // Process each 64-byte chunk
        var W = new Array(64);
        for (var chunk = 0; chunk < padded.length; chunk += 64) {
            // Create message schedule
            for (var t = 0; t < 16; t++) {
                W[t] = (padded[chunk + t * 4] << 24) | (padded[chunk + t * 4 + 1] << 16) |
                       (padded[chunk + t * 4 + 2] << 8) | padded[chunk + t * 4 + 3];
            }
            for (var t = 16; t < 64; t++) {
                var s0 = rightRotate(W[t - 15], 7) ^ rightRotate(W[t - 15], 18) ^ (W[t - 15] >>> 3);
                var s1 = rightRotate(W[t - 2], 17) ^ rightRotate(W[t - 2], 19) ^ (W[t - 2] >>> 10);
                W[t] = (W[t - 16] + s0 + W[t - 7] + s1) >>> 0;
            }
            
            // Initialize working variables
            var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            
            // Main loop
            for (var t = 0; t < 64; t++) {
                var S1 = rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25);
                var ch = (e & f) ^ (~e & g);
                var temp1 = (h + S1 + ch + K[t] + W[t]) >>> 0;
                var S0 = rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22);
                var maj = (a & b) ^ (a & c) ^ (b & c);
                var temp2 = (S0 + maj) >>> 0;
                
                h = g; g = f; f = e; e = (d + temp1) >>> 0;
                d = c; c = b; b = a; a = (temp1 + temp2) >>> 0;
            }
            
            // Add compressed chunk to hash
            H[0] = (H[0] + a) >>> 0; H[1] = (H[1] + b) >>> 0;
            H[2] = (H[2] + c) >>> 0; H[3] = (H[3] + d) >>> 0;
            H[4] = (H[4] + e) >>> 0; H[5] = (H[5] + f) >>> 0;
            H[6] = (H[6] + g) >>> 0; H[7] = (H[7] + h) >>> 0;
        }
        
        // Produce final hash
        var result = new Uint8Array(32);
        for (var i = 0; i < 8; i++) {
            result[i * 4] = (H[i] >>> 24) & 0xff;
            result[i * 4 + 1] = (H[i] >>> 16) & 0xff;
            result[i * 4 + 2] = (H[i] >>> 8) & 0xff;
            result[i * 4 + 3] = H[i] & 0xff;
        }
        return result.buffer;
    };
    
    // =========================================================================
    // SHA-1 Implementation (for compatibility)
    // =========================================================================
    var sha1 = function(message) {
        var H = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];
        
        var leftRotate = function(n, s) { return ((n << s) | (n >>> (32 - s))) >>> 0; };
        
        var bytes;
        if (typeof message === 'string') {
            bytes = new Uint8Array(new TextEncoder().encode(message));
        } else if (message instanceof ArrayBuffer) {
            bytes = new Uint8Array(message);
        } else {
            bytes = new Uint8Array(message);
        }
        
        var msgLen = bytes.length;
        var bitLen = msgLen * 8;
        var padLen = ((msgLen % 64) < 56) ? (56 - msgLen % 64) : (120 - msgLen % 64);
        
        var padded = new Uint8Array(msgLen + padLen + 8);
        padded.set(bytes);
        padded[msgLen] = 0x80;
        
        var lenPos = padded.length - 8;
        for (var i = 0; i < 8; i++) {
            padded[lenPos + i] = (i < 4) ? 0 : ((bitLen >>> ((7 - i) * 8)) & 0xff);
        }
        
        var W = new Array(80);
        for (var chunk = 0; chunk < padded.length; chunk += 64) {
            for (var t = 0; t < 16; t++) {
                W[t] = (padded[chunk + t * 4] << 24) | (padded[chunk + t * 4 + 1] << 16) |
                       (padded[chunk + t * 4 + 2] << 8) | padded[chunk + t * 4 + 3];
            }
            for (var t = 16; t < 80; t++) {
                W[t] = leftRotate(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16], 1);
            }
            
            var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4];
            
            for (var t = 0; t < 80; t++) {
                var f, k;
                if (t < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (t < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }
                
                var temp = (leftRotate(a, 5) + f + e + k + W[t]) >>> 0;
                e = d; d = c; c = leftRotate(b, 30); b = a; a = temp;
            }
            
            H[0] = (H[0] + a) >>> 0; H[1] = (H[1] + b) >>> 0;
            H[2] = (H[2] + c) >>> 0; H[3] = (H[3] + d) >>> 0; H[4] = (H[4] + e) >>> 0;
        }
        
        var result = new Uint8Array(20);
        for (var i = 0; i < 5; i++) {
            result[i * 4] = (H[i] >>> 24) & 0xff;
            result[i * 4 + 1] = (H[i] >>> 16) & 0xff;
            result[i * 4 + 2] = (H[i] >>> 8) & 0xff;
            result[i * 4 + 3] = H[i] & 0xff;
        }
        return result.buffer;
    };
    
    // =========================================================================
    // SubtleCrypto Implementation
    // =========================================================================
    var subtleCrypto = {
        digest: function(algorithm, data) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    algoName = algoName.toUpperCase().replace('-', '');
                    
                    var bytes;
                    if (data instanceof ArrayBuffer) {
                        bytes = data;
                    } else if (ArrayBuffer.isView(data)) {
                        bytes = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
                    } else {
                        reject(new Error('Invalid data type for digest'));
                        return;
                    }
                    
                    if (algoName === 'SHA256' || algoName === 'SHA-256') {
                        resolve(sha256(bytes));
                    } else if (algoName === 'SHA1' || algoName === 'SHA-1') {
                        resolve(sha1(bytes));
                    } else {
                        reject(new Error('Unsupported algorithm: ' + algoName));
                    }
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Generate random key (simplified)
        generateKey: function(algorithm, extractable, keyUsages) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    var keyLength = algorithm.length || 256;
                    var keyBytes = new Uint8Array(keyLength / 8);
                    getRandomValues(keyBytes);
                    
                    resolve({
                        type: 'secret',
                        extractable: extractable,
                        algorithm: { name: algoName, length: keyLength },
                        usages: keyUsages,
                        _keyData: keyBytes
                    });
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Import key (simplified)
        importKey: function(format, keyData, algorithm, extractable, keyUsages) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    var bytes;
                    if (format === 'raw') {
                        if (keyData instanceof ArrayBuffer) {
                            bytes = new Uint8Array(keyData);
                        } else if (ArrayBuffer.isView(keyData)) {
                            bytes = new Uint8Array(keyData.buffer, keyData.byteOffset, keyData.byteLength);
                        }
                    } else if (format === 'jwk') {
                        // Handle JWK format - extract k value (base64url encoded)
                        if (keyData.k) {
                            var base64 = keyData.k.replace(/-/g, '+').replace(/_/g, '/');
                            var binary = atob(base64);
                            bytes = new Uint8Array(binary.length);
                            for (var i = 0; i < binary.length; i++) {
                                bytes[i] = binary.charCodeAt(i);
                            }
                        }
                    }
                    
                    resolve({
                        type: 'secret',
                        extractable: extractable,
                        algorithm: { name: algoName },
                        usages: keyUsages,
                        _keyData: bytes
                    });
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Export key
        exportKey: function(format, key) {
            return new Promise(function(resolve, reject) {
                try {
                    if (format === 'raw' && key._keyData) {
                        resolve(key._keyData.buffer);
                    } else {
                        reject(new Error('Unsupported export format'));
                    }
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Sign with HMAC (simplified)
        sign: function(algorithm, key, data) {
            return new Promise(function(resolve, reject) {
                try {
                    // Simple HMAC-SHA256 implementation
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    if (algoName !== 'HMAC') {
                        reject(new Error('Only HMAC signing is supported'));
                        return;
                    }
                    
                    var keyBytes = key._keyData;
                    var msgBytes;
                    if (data instanceof ArrayBuffer) {
                        msgBytes = new Uint8Array(data);
                    } else if (ArrayBuffer.isView(data)) {
                        msgBytes = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
                    }
                    
                    // HMAC: H((K XOR opad) || H((K XOR ipad) || message))
                    var blockSize = 64;
                    var opad = new Uint8Array(blockSize);
                    var ipad = new Uint8Array(blockSize);
                    
                    // If key > blockSize, hash it first
                    if (keyBytes.length > blockSize) {
                        keyBytes = new Uint8Array(sha256(keyBytes));
                    }
                    
                    for (var i = 0; i < blockSize; i++) {
                        var k = i < keyBytes.length ? keyBytes[i] : 0;
                        opad[i] = k ^ 0x5c;
                        ipad[i] = k ^ 0x36;
                    }
                    
                    // Inner hash: H(ipad || message)
                    var innerData = new Uint8Array(blockSize + msgBytes.length);
                    innerData.set(ipad);
                    innerData.set(msgBytes, blockSize);
                    var innerHash = new Uint8Array(sha256(innerData));
                    
                    // Outer hash: H(opad || innerHash)
                    var outerData = new Uint8Array(blockSize + 32);
                    outerData.set(opad);
                    outerData.set(innerHash, blockSize);
                    
                    resolve(sha256(outerData));
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        verify: function() { return Promise.reject(new Error('verify not implemented')); },
        encrypt: function() { return Promise.reject(new Error('encrypt not implemented')); },
        decrypt: function() { return Promise.reject(new Error('decrypt not implemented')); },
        deriveBits: function() { return Promise.reject(new Error('deriveBits not implemented')); },
        deriveKey: function() { return Promise.reject(new Error('deriveKey not implemented')); },
        wrapKey: function() { return Promise.reject(new Error('wrapKey not implemented')); },
        unwrapKey: function() { return Promise.reject(new Error('unwrapKey not implemented')); }
    };
    
    // Create a complete crypto polyfill object
    var cryptoPolyfill = {
        getRandomValues: getRandomValues,
        randomUUID: randomUUID,
        subtle: subtleCrypto
    };
    
    // Force override the crypto object to avoid "secure origin" errors
    // We need to delete and recreate because crypto might be non-configurable
    try {
        // Try to override directly
        Object.defineProperty(window, 'crypto', {
            value: cryptoPolyfill,
            writable: true,
            configurable: true,
            enumerable: true
        });
    } catch (e) {
        // If that fails, try to override individual methods
        try {
            window.crypto.getRandomValues = getRandomValues;
            window.crypto.randomUUID = randomUUID;
            window.crypto.subtle = subtleCrypto;
        } catch (e2) {
            // Last resort: create a new global
            window.myCrypto = cryptoPolyfill;
            console.log('WebCrypto polyfill: Could not override crypto, use window.myCrypto instead');
        }
    }
    
    // Also expose as globalThis.crypto for modules
    if (typeof globalThis !== 'undefined') {
        try {
            Object.defineProperty(globalThis, 'crypto', {
                value: cryptoPolyfill,
                writable: true,
                configurable: true,
                enumerable: true
            });
        } catch (e) {}
    }
    
    console.log('WebCrypto polyfill loaded with SHA-256, SHA-1, HMAC support');
})();
)";
        
        main_view_->EvaluateScript(webcrypto_patch.c_str());
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