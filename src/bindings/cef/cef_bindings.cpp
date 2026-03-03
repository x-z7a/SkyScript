#include "cef_bindings.h"

#ifdef SKYSCRIPT_CEF_ENABLED

#include "include/cef_v8.h"
#include "include/cef_process_message.h"
#include "log_msg.h"

// ────────────────────────────────────────────────────────────────────────────
// TODO (Phase 7): Implement full V8 ↔ IPC binding.
//
// Outline of what needs to happen here:
//
//   1. CefBindingsInjectContext() is called from OnContextCreated in the
//      renderer subprocess (SkyScript_CEF_Helper).  It creates:
//        - window.XPlane           (namespace object)
//        - window.XPlane.dataref   (V8 object backed by CefXPlaneDatarefHandler)
//        - window.SkyScript        (namespace object)
//        ... (other sub-namespaces mirroring the Ultralight bindings)
//
//   2. When JS calls e.g. XPlane.dataref.getFloat("sim/.../altitude"):
//        a. CefXPlaneDatarefHandler::Execute() is called in the renderer.
//        b. It creates a CefProcessMessage("xplane_request") with args
//           [request_id, "dataref.getFloat", path].
//        c. It returns a JS Promise and stores {request_id → resolve/reject}.
//        d. SendProcessMessage(PID_BROWSER, message) sends it to the browser.
//
//   3. In the browser process, CefAppClient::OnProcessMessageReceived receives
//      "xplane_request", dispatches to XPlaneBridge::GetFloat(), and sends
//      back "xplane_response" with [request_id, result_value].
//
//   4. Back in the renderer, OnProcessMessageReceived looks up the pending
//      Promise by request_id and calls resolve(result_value).
//
// Synchronous calls (blocking renderer thread) are deliberately avoided —
// they would deadlock the CEF message loop.
// ────────────────────────────────────────────────────────────────────────────

void CefBindingsInjectContext(CefRefPtr<CefBrowser>   browser,
                              CefRefPtr<CefFrame>     frame,
                              CefRefPtr<CefV8Context> context,
                              const std::string &app_name)
{
    // TODO: inject V8 objects
    LogMsg("[%s] CefBindings: context created — V8 injection not yet implemented",
           app_name.c_str());
    (void)browser; (void)frame; (void)context;
}

bool CefBindingsHandleResponseMessage(CefRefPtr<CefBrowser>        browser,
                                      CefRefPtr<CefFrame>          frame,
                                      CefProcessId                 source_process,
                                      CefRefPtr<CefProcessMessage> message)
{
    // TODO: resolve/reject pending JS Promises
    (void)browser; (void)frame; (void)source_process; (void)message;
    return false;
}

#endif // SKYSCRIPT_CEF_ENABLED
