#pragma once

#include <string>

#include "log_msg.h"

#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMProcessing.h>

#if APL
#include <OpenGL/gl.h>
#elif LIN || IBM
#include <GL/gl.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

/**
 * @brief Abstract base class for all SkyScript app renderers.
 *
 * Owns the X-Plane window (XPLMWindowID) and the OpenGL texture quad draw
 * pipeline. Subclasses (UltralightApp, CefApp) implement renderer-specific
 * Initialize/Destroy/UpdateTexture/ForceRepaint/Reload and event routing.
 *
 * Thread safety: all methods must be called from X-Plane's main thread.
 */
class AppBase
{
public:
    AppBase();
    AppBase(const std::string &name, const std::string &dir);
    virtual ~AppBase();

    // Non-copyable
    AppBase(const AppBase &) = delete;
    AppBase &operator=(const AppBase &) = delete;

    // ── Pure virtuals ────────────────────────────────────────────────────────
    /** Initialize the renderer and create the X-Plane window. */
    virtual void Initialize() = 0;

    /** Destroy the renderer, close the window, and free the GL texture. */
    virtual void Destroy() = 0;

    /** Upload any pending frame data to the GL texture (called every frame). */
    virtual void UpdateTexture() = 0;

    /** Mark the renderer's backing surface as dirty so it re-renders. */
    virtual void ForceRepaint() = 0;

    /** Reload the app's index.html in-place. */
    virtual void Reload() = 0;

    /** Returns true once Initialize() has completed successfully. */
    virtual bool IsInitialized() const = 0;

    // ── Inspector virtuals (default: no-op) ─────────────────────────────────
    virtual void ShowInspector() {}
    virtual void HideInspector() {}
    virtual void ToggleInspector();
    virtual bool IsInspectorVisible() const { return false; }
    virtual void UpdateInspectorTexture() {}
    virtual void DrawInspector() {}

    // ── Input event virtuals (default: consume nothing) ─────────────────────
    virtual int  OnMouseClick(int x, int y, int button, int mouseStatus) { return 0; }
    virtual int  OnMouseMove(int x, int y) { return 0; }
    virtual int  OnMouseWheel(int x, int y, int wheel, int clicks) { return 0; }
    virtual void OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus) {}

    // ── Shared window management ─────────────────────────────────────────────
    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;

    /**
     * Draw the current texture as a full-window quad.
     * Calls CheckResize() then UpdateTexture() before drawing.
     */
    void Draw();

    // ── Getters ───────────────────────────────────────────────────────────────
    const std::string &GetName() const        { return app_name_; }
    const std::string &GetDisplayName() const { return app_display_name_; }

protected:
    /**
     * Create the XPLMWindowEx with all input callbacks wired up.
     * Called by subclass Initialize() after the renderer is ready.
     */
    void CreateXPlaneWindow();

    /**
     * Called by CheckResize() when the window dimensions have changed.
     * Subclasses resize their backing view/browser here.
     */
    virtual void OnWindowResized(int new_width, int new_height) = 0;

    std::string app_name_;
    std::string app_display_name_;
    std::string app_dir_;
    XPLMWindowID main_window_ = nullptr;
    GLuint  texture_id_  = 0;
    int     view_width_  = 800;
    int     view_height_ = 600;

private:
    void CheckResize();
    int resize_frame_skip_ = 0;
};
