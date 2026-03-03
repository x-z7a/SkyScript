#include "app_base.h"
#include "manifest_config.h"

#include <cstring>

AppBase::AppBase() : app_name_(""), app_display_name_(""), app_dir_("") {}

AppBase::AppBase(const std::string &name, const std::string &dir)
    : app_name_(name), app_display_name_(name), app_dir_(dir)
{
    ManifestConfig config = ReadManifestConfig(app_dir_ + "/manifest.json");
    if (!config.short_name.empty())
    {
        app_display_name_ = config.short_name;
    }
    view_width_  = config.width;
    view_height_ = config.height;
    LogMsg("AppBase created: %s, dir: %s", app_name_.c_str(), app_dir_.c_str());
}

AppBase::~AppBase()
{
    // Subclass Destroy() should have already run. Guard against dangling window.
    if (main_window_)
    {
        LogMsg("[%s] AppBase destructor: destroying orphan window", app_name_.c_str());
        XPLMDestroyWindow(main_window_);
        main_window_ = nullptr;
    }
    if (texture_id_ != 0)
    {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
}

// ── Window management ─────────────────────────────────────────────────────────

void AppBase::Show()
{
    if (main_window_)
    {
        XPLMSetWindowIsVisible(main_window_, 1);
        XPLMBringWindowToFront(main_window_);
    }
}

void AppBase::Hide()
{
    if (main_window_)
    {
        XPLMSetWindowIsVisible(main_window_, 0);
    }
}

void AppBase::Toggle()
{
    if (main_window_)
    {
        if (XPLMGetWindowIsVisible(main_window_))
            Hide();
        else
            Show();
    }
}

bool AppBase::IsVisible() const
{
    return main_window_ && XPLMGetWindowIsVisible(main_window_);
}

void AppBase::ToggleInspector()
{
    if (IsInspectorVisible())
        HideInspector();
    else
        ShowInspector();
}

// ── GL quad draw ──────────────────────────────────────────────────────────────

void AppBase::Draw()
{
    if (!main_window_)
        return;

    CheckResize();
    UpdateTexture();

    if (texture_id_ == 0)
        return;

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
    // Renderer delivers top-down bitmaps; OpenGL is bottom-up.
    // V=0 at top of quad (top of screen), V=1 at bottom.
    glTexCoord2f(0.0f, 0.0f); glVertex2f(left,  top);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(left,  bottom);
    glEnd();
}

// ── Resize detection ──────────────────────────────────────────────────────────

void AppBase::CheckResize()
{
    if (!main_window_)
        return;

    if (++resize_frame_skip_ < 10)
        return;
    resize_frame_skip_ = 0;

    int left, top, right, bottom;
    XPLMGetWindowGeometry(main_window_, &left, &top, &right, &bottom);

    int new_width  = right - left;
    int new_height = top   - bottom;

    if (new_width != view_width_ || new_height != view_height_)
    {
        LogMsg("[%s] Window resized: %dx%d -> %dx%d",
               app_name_.c_str(), view_width_, view_height_, new_width, new_height);

        view_width_  = new_width;
        view_height_ = new_height;

        // Delete old texture so a correctly-sized one is created on next UpdateTexture().
        if (texture_id_ != 0)
        {
            glDeleteTextures(1, &texture_id_);
            texture_id_ = 0;
        }

        OnWindowResized(new_width, new_height);
    }
}

// ── X-Plane window creation ───────────────────────────────────────────────────

void AppBase::CreateXPlaneWindow()
{
    int winLeft, winTop, winRight, winBot;
    XPLMGetScreenBoundsGlobal(&winLeft, &winTop, &winRight, &winBot);

    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.left   = winLeft + 100;
    params.right  = winLeft + 100 + view_width_;
    params.top    = winTop  - 100;
    params.bottom = winTop  - 100 - view_height_;
    params.visible = 1;
    params.refcon  = this;

    params.drawWindowFunc = [](XPLMWindowID /*wnd*/, void *refcon)
    {
        static_cast<AppBase *>(refcon)->Draw();
    };

    params.handleMouseClickFunc = [](XPLMWindowID /*wnd*/, int x, int y, int isDown, void *refcon) -> int
    {
        return static_cast<AppBase *>(refcon)->OnMouseClick(x, y, 0, isDown);
    };

    params.handleRightClickFunc = [](XPLMWindowID /*wnd*/, int x, int y, int isDown, void *refcon) -> int
    {
        return static_cast<AppBase *>(refcon)->OnMouseClick(x, y, 1, isDown);
    };

    params.handleMouseWheelFunc = [](XPLMWindowID /*wnd*/, int x, int y, int wheel, int clicks, void *refcon) -> int
    {
        return static_cast<AppBase *>(refcon)->OnMouseWheel(x, y, wheel, clicks);
    };

    params.handleKeyFunc = [](XPLMWindowID /*wnd*/, char key, XPLMKeyFlags flags, char virtualKey, void *refcon, int losingFocus)
    {
        static_cast<AppBase *>(refcon)->OnKey(key, flags, virtualKey, losingFocus);
    };

    params.handleCursorFunc = [](XPLMWindowID /*wnd*/, int x, int y, void *refcon) -> XPLMCursorStatus
    {
        static_cast<AppBase *>(refcon)->OnMouseMove(x, y);
        return xplm_CursorDefault;
    };

    params.layer = xplm_WindowLayerFloatingWindows;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    main_window_ = XPLMCreateWindowEx(&params);
    XPLMSetWindowTitle(main_window_, app_display_name_.c_str());
    XPLMSetWindowResizingLimits(main_window_, 200, 200, 2000, 2000);

    // VR mode support
    XPLMDataRef vr_ref = XPLMFindDataRef("sim/graphics/VR/enabled");
    int vr_enabled = vr_ref ? XPLMGetDatai(vr_ref) : 0;
    if (vr_enabled)
        XPLMSetWindowPositioningMode(main_window_, xplm_WindowVR, -1);
    else
        XPLMSetWindowPositioningMode(main_window_, xplm_WindowPositionFree, -1);

    XPLMSetWindowIsVisible(main_window_, 0); // Hidden by default — use menu to show
}
