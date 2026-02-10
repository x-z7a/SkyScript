#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "log_msg.h"
#include "bindings/bindings.h"

#include <Ultralight/Ultralight.h>
#include <JavaScriptCore/JavaScript.h>
#include <AppCore/Platform.h>
#include <AppCore/JSHelpers.h>

#include <XPLMDisplay.h>
#include <XPLMPlugin.h>
#include <XPLMMenus.h>
#include <XPLMProcessing.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>

#if APL
#include <OpenGL/gl.h>
#elif LIN || IBM
#include <GL/gl.h>
#endif

// Windows gl.h doesn't include these OpenGL 1.2+ constants
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

using namespace ultralight;

class App : public ultralight::ViewListener, public ultralight::LoadListener
{
public:
    App();
    App(const std::string &name, const std::string &dir);
    ~App();

    // Delete copy constructor and copy assignment operator
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    void Initialize();
    void Destroy();        // Cleanup view and window resources
    void UpdateTexture();  // Update texture from Ultralight bitmap
    void Draw();
    void ProcessPendingMainThreadWork();
    
    // Window visibility
    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;
    bool IsInitialized() const { return main_view_ready_.load(); }
    
    // Reload the view (refresh HTML/JS)
    void Reload();
    
    // Inspector support
    void ShowInspector();
    void HideInspector();
    void ToggleInspector();
    bool IsInspectorVisible() const;
    void UpdateInspectorTexture();
    void DrawInspector();
    void CheckInspectorResize();
    void CreateInspectorWindow();
    int OnInspectorMouseClick(int x, int y, int button, int mouseStatus);
    int OnInspectorMouseWheel(int clicks);
    int OnInspectorMouseMove(int x, int y);
    void OnInspectorKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus);
    
    // Getters
    const std::string& GetName() const { return app_name; }
    const std::string& GetDisplayName() const { return app_display_name; }
    
    // Force the view to repaint
    void ForceRepaint();
    void ForceRepaintOnUltralightThread();
    void CaptureSurfacesOnUltralightThread();
    
    // Mouse event handlers
    int OnMouseClick(int x, int y, int button, int mouseStatus);
    int OnMouseWheel(int clicks);
    int OnMouseMove(int x, int y);
    void OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus);
    void CheckResize();

    // LoadListener overrides
    virtual void OnAddConsoleMessage(View *caller, const ConsoleMessage &msg) override;
    virtual void OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    virtual void OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    virtual void OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url, const String &description, const String &error_domain, int error_code) override;
    virtual void OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;

    // ViewListener overrides
    virtual RefPtr<View> OnCreateInspectorView(View *caller, bool is_local, const String &inspected_url) override;

private:
    struct StagedSurface
    {
        std::mutex mutex;
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        bool has_new_frame = false;
    };

    void CaptureSurface(const RefPtr<View>& view, StagedSurface& staged_surface);
    void UploadStagedSurface(StagedSurface& staged_surface, GLuint& texture_id, int& texture_width, int& texture_height);
    void InitializeUltralightOnThread();
    void DestroyUltralightOnThread();

    std::string app_name;
    std::string app_display_name;
    std::string app_dir;
    RefPtr<Renderer> renderer_;
    RefPtr<Session> session_;  // Per-app session for isolated storage (cookies, localStorage, etc.)
    RefPtr<View> main_view_;
    XPLMWindowID main_window_ = nullptr;
    GLuint texture_id_ = 0;
    int view_width_ = 800;
    int view_height_ = 600;
    int texture_width_ = 0;
    int texture_height_ = 0;
    std::atomic<bool> main_view_ready_{false};
    StagedSurface main_staged_surface_;
    
    // Inspector support
    RefPtr<View> inspector_view_;
    XPLMWindowID inspector_window_ = nullptr;
    GLuint inspector_texture_id_ = 0;
    int inspector_width_ = 800;
    int inspector_height_ = 600;
    int inspector_texture_width_ = 0;
    int inspector_texture_height_ = 0;
    std::atomic<bool> inspector_pending_{false};
    std::atomic<bool> inspector_view_ready_{false};
    std::atomic<bool> inspector_window_requested_{false};
    StagedSurface inspector_staged_surface_;
};
