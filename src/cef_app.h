#pragma once

#include "app_base.h"

#ifdef SKYSCRIPT_CEF_ENABLED

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"

#include <atomic>
#include <mutex>
#include <vector>

// ── Forward declarations ──────────────────────────────────────────────────────
class CefAppClient;
class SkyScriptRenderHandler;

/**
 * @brief CEF Off-Screen Rendering (OSR) backed SkyScript app.
 *
 * Uses CefBrowserHost with windowless rendering enabled.  CEF delivers BGRA
 * pixel data via CefRenderHandler::OnPaint(); this class uploads it to an
 * OpenGL texture and delegates the quad draw to AppBase::Draw().
 *
 * Thread safety:
 *   - All XPLM / OpenGL calls happen on X-Plane's main thread.
 *   - X-Plane drives CEF's message loop internally, so OnPaint() arrives on
 *     the main thread — no locking needed for the pixel buffer.
 *
 * Requires SKYSCRIPT_CEF_ENABLED to be defined and CEF SDK headers available.
 * The CEF SDK is downloaded by scripts/fetch-cef.sh.
 */
class CefApp : public AppBase
{
public:
    CefApp();
    CefApp(const std::string &name, const std::string &dir);
    ~CefApp() override;

    CefApp(const CefApp &) = delete;
    CefApp &operator=(const CefApp &) = delete;

    // ── AppBase interface ─────────────────────────────────────────────────────
    void Initialize() override;
    void Destroy() override;
    void UpdateTexture() override;
    void ForceRepaint() override;
    void Reload() override;
    bool IsInitialized() const override;

    // ── Input (Phase 6) ───────────────────────────────────────────────────────
    int  OnMouseClick(int x, int y, int button, int mouseStatus) override;
    int  OnMouseMove(int x, int y) override;
    int  OnMouseWheel(int x, int y, int wheel, int clicks) override;
    void OnKey(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus) override;

protected:
    void OnWindowResized(int new_width, int new_height) override;

private:
    // Pixel buffer written by SkyScriptRenderHandler::OnPaint() on the main
    // thread, consumed by UpdateTexture() also on the main thread.
    std::vector<uint8_t> pixel_buffer_;
    bool has_new_frame_ = false;
    int  paint_width_   = 0;
    int  paint_height_  = 0;

    CefRefPtr<CefBrowser>   browser_;
    CefRefPtr<CefAppClient> client_;

    bool initialized_ = false;

    friend class SkyScriptRenderHandler;
    friend class CefAppClient;
};

// ── CefRenderHandler ──────────────────────────────────────────────────────────

/**
 * Receives CEF's OSR pixel data and stores it for upload on the next frame.
 */
class SkyScriptRenderHandler : public CefRenderHandler
{
public:
    explicit SkyScriptRenderHandler(CefApp *owner) : owner_(owner) {}

    // CefRenderHandler
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList &dirtyRects,
                 const void *buffer,
                 int width, int height) override;

private:
    CefApp *owner_;
    IMPLEMENT_REFCOUNTING(SkyScriptRenderHandler);
};

// ── CefClient ─────────────────────────────────────────────────────────────────

class CefAppClient
    : public CefClient
    , public CefLifeSpanHandler
    , public CefLoadHandler
    , public CefDisplayHandler
{
public:
    explicit CefAppClient(CefApp *owner);

    // CefClient
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return render_handler_; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler>     GetLoadHandler()     override { return this; }
    CefRefPtr<CefDisplayHandler>  GetDisplayHandler()  override { return this; }

    // CefLifeSpanHandler
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString &errorText,
                     const CefString &failedUrl) override;

    // CefDisplayHandler
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString &message,
                          const CefString &source,
                          int line) override;

private:
    CefApp *owner_;
    CefRefPtr<SkyScriptRenderHandler> render_handler_;

    IMPLEMENT_REFCOUNTING(CefAppClient);
};

#else // SKYSCRIPT_CEF_ENABLED not defined ──────────────────────────────────────

/**
 * Stub CefApp used when CEF support is not compiled in.
 * discoverApps() in manager.cpp falls back to UltralightApp before ever
 * instantiating this class, so these stubs are never called in practice.
 */
class CefApp : public AppBase
{
public:
    CefApp()  = default;
    CefApp(const std::string &name, const std::string &dir) : AppBase(name, dir) {}
    ~CefApp() override = default;

    void Initialize() override
    {
        LogMsg("[%s] CefApp::Initialize: CEF not compiled in", app_name_.c_str());
    }
    void Destroy()        override {}
    void UpdateTexture()  override {}
    void ForceRepaint()   override {}
    void Reload()         override {}
    bool IsInitialized() const override { return false; }

protected:
    void OnWindowResized(int, int) override {}
};

#endif // SKYSCRIPT_CEF_ENABLED
