#pragma once
/**
 * cef_dataref.h — CEF V8 handler for XPlane.dataref.* bindings.
 *
 * Current status: STUB.
 * TODO (Phase 7): Implement CefV8Handler that translates:
 *   - XPlane.dataref.getFloat(path)   → Promise<float>
 *   - XPlane.dataref.setFloat(path, v)→ Promise<void>
 *   - XPlane.dataref.getDouble(path)  → Promise<double>
 *   - XPlane.dataref.getInt(path)     → Promise<int>
 *   - XPlane.dataref.setInt(path, v)  → Promise<void>
 *   - XPlane.dataref.getFloatArray(path, count, offset) → Promise<Float32Array>
 *
 * Each call sends a CefProcessMessage to the browser process which calls
 * the corresponding XPlaneBridge helper and returns the result.
 */

#ifdef SKYSCRIPT_CEF_ENABLED
#include "include/cef_v8.h"
// TODO: add CefDatarefV8Handler class declaration
#endif
