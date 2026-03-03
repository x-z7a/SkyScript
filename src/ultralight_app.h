#pragma once

#include "app_base.h"

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/Platform.h>
#include <AppCore/JSHelpers.h>

using namespace ultralight;

/**
 * @brief Ultralight-backed SkyScript app renderer.
 *
 * Uses Ultralight 1.4.0 Off-Screen Rendering: renders HTML/CSS/JS to a CPU
 * bitmap (BitmapSurface), then uploads to an OpenGL texture via the shared
 * AppBase::Draw() quad pipeline.
 */
class UltralightApp
    : public AppBase
    , public ultralight::ViewListener
    , public ultralight::LoadListener
{
public:
    UltralightApp();
    UltralightApp(const std::string &name, const std::string &dir);
    ~UltralightApp() override;

    UltralightApp(const UltralightApp &) = delete;
    UltralightApp &operator=(const UltralightApp &) = delete;

    // ── AppBase interface ─────────────────────────────────────────────────────
    void Initialize() override;
    void Destroy() override;
    void UpdateTexture() override;
    void ForceRepaint() override;
    void Reload() override;
    bool IsInitialized() const override { return main_view_.get() != nullptr; }

    // ── Inspector ─────────────────────────────────────────────────────────────
    void ShowInspector() override;
    void HideInspector() override;
    void ToggleInspector() override;
    bool IsInspectorVisible() const override;
    void UpdateInspectorTexture() override;
    void DrawInspector() override;

    // ── Input ─────────────────────────────────────────────────────────────────
    int  OnMouseClick(int x, int y, int button, int mouseStatus) override;
    int  OnMouseMove(int x, int y) override;
    int  OnMouseWheel(int x, int y, int wheel, int clicks) override;
    void OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus) override;

    // ── Ultralight LoadListener overrides ─────────────────────────────────────
    void OnAddConsoleMessage(View *caller, const ConsoleMessage &msg) override;
    void OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    void OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    void OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame,
                       const String &url, const String &description,
                       const String &error_domain, int error_code) override;
    void OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;

    // ── Ultralight ViewListener overrides ────────────────────────────────────
    RefPtr<View> OnCreateInspectorView(View *caller, bool is_local,
                                       const String &inspected_url) override;

protected:
    void OnWindowResized(int new_width, int new_height) override;

private:
    void CreateInspectorWindow();
    void CheckInspectorResize();
    int  OnInspectorMouseClick(int x, int y, int button, int mouseStatus);
    int  OnInspectorMouseMove(int x, int y);
    void OnInspectorKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus);

    RefPtr<Renderer> renderer_;
    RefPtr<Session>  session_;
    RefPtr<View>     main_view_;

    // Inspector
    RefPtr<View>  inspector_view_;
    XPLMWindowID  inspector_window_      = nullptr;
    GLuint        inspector_texture_id_  = 0;
    int           inspector_width_       = 800;
    int           inspector_height_      = 600;
    bool          inspector_pending_     = false;
    int           inspector_frame_skip_  = 0;
};
