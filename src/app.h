
#include <string>

#include "log_msg.h"

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

    void Initialize(RefPtr<Renderer> renderer);
    void UpdateTexture();  // Update texture from Ultralight bitmap
    void Draw();
    
    // Window visibility
    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;
    
    // Getters
    const std::string& GetName() const { return app_name; }
    
    // Force the view to repaint
    void ForceRepaint();
    
    // Mouse event handlers
    int OnMouseClick(int x, int y, int button, int mouseStatus);
    int OnMouseMove(int x, int y);
    void OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus);
    void CheckResize();

    // LoadListener overrides
    virtual void OnAddConsoleMessage(View *caller, const ConsoleMessage &msg) override;
    virtual void OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    virtual void OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;
    virtual void OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url, const String &description, const String &error_domain, int error_code) override;
    virtual void OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url) override;

private:
    std::string app_name;
    std::string app_dir;
    RefPtr<View> main_view_;
    XPLMWindowID main_window_ = nullptr;
    GLuint texture_id_ = 0;
    int view_width_ = 800;
    int view_height_ = 600;
    bool texture_needs_init_ = true;
};
