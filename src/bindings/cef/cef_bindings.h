#pragma once
/**
 * cef_bindings.h — CEF V8 binding coordinator.
 *
 * Injects XPlane.* and SkyScript.* objects into a CEF renderer process V8
 * context using CefV8Handler subclasses.
 *
 * Architecture (CEF multi-process IPC model):
 *
 *   Browser process (X-Plane main thread)
 *     └── CefApp (cef_app.h) — hosts CefBrowser
 *           └── Receives CefProcessMessage from renderer
 *                 └── Calls XPlaneBridge helpers
 *                       └── Sends response CefProcessMessage back
 *
 *   Renderer process (SkyScript_CEF_Helper subprocess)
 *     └── CefV8Handler — handles XPlane.dataref.getFloat(...) calls
 *           └── Sends CefProcessMessage to browser process
 *                 └── Callback fires JS Promise resolve/reject
 *
 * All X-Plane SDK calls happen in the browser process (X-Plane main thread).
 * JS bindings in the renderer process are inherently async (Promise-based).
 *
 * Current status: STUB — IPC wiring not yet implemented.
 * TODO (Phase 7 completion):
 *   1. Implement CefXPlaneV8Handler with IPC send.
 *   2. Implement browser-side message handler in CefAppClient.
 *   3. Wire response back via SendProcessMessage to renderer.
 *   4. Resolve/reject JS Promise in the renderer-side callback.
 */

#ifdef SKYSCRIPT_CEF_ENABLED

#include "include/cef_v8.h"
#include "include/cef_render_process_handler.h"

#include <string>

/**
 * @brief Called from CefRenderProcessHandler::OnContextCreated.
 *
 * Injects the XPlane and SkyScript namespace objects into the given V8 context.
 * All JS API calls are dispatched asynchronously via IPC to the browser process.
 *
 * @param browser  The browser associated with this context.
 * @param frame    The frame whose context was created.
 * @param context  The V8 context to inject into.
 * @param app_name Internal app name (passed from browser process via bind message).
 */
void CefBindingsInjectContext(CefRefPtr<CefBrowser>  browser,
                              CefRefPtr<CefFrame>    frame,
                              CefRefPtr<CefV8Context> context,
                              const std::string &app_name);

/**
 * @brief Handle a response message from the browser process.
 *
 * Called from CefRenderProcessHandler::OnProcessMessageReceived.
 * Resolves or rejects the pending JS Promise associated with the request.
 */
bool CefBindingsHandleResponseMessage(CefRefPtr<CefBrowser>        browser,
                                      CefRefPtr<CefFrame>          frame,
                                      CefProcessId                 source_process,
                                      CefRefPtr<CefProcessMessage> message);

#endif // SKYSCRIPT_CEF_ENABLED
