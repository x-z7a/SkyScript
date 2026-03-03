#pragma once
/**
 * xplane_bridge.h — Plain C++ wrappers around X-Plane SDK calls.
 *
 * This header provides renderer-agnostic helpers used by both the Ultralight
 * JSBindings layer and the (future) CEF V8 bindings layer.  No JS types are
 * referenced here — only X-Plane SDK types and standard C++.
 *
 * Keeping the X-Plane calls here means the CEF renderer process can call into
 * X-Plane via IPC without duplicating the SDK call logic.
 *
 * Current status: stub / placeholder.
 * TODO (Phase 7): extract actual X-Plane calls from dataref.cpp, scenery.cpp,
 *                 instance.cpp, graphics.cpp, utilities.cpp into inline helpers
 *                 here, then call them from both the Ultralight JSBindings and
 *                 the CEF IPC message handler.
 */

#include <string>
#include <vector>

#include <XPLMDataAccess.h>
#include <XPLMScenery.h>
#include <XPLMPlugin.h>

namespace XPlaneBridge
{

// ── DataRef helpers ───────────────────────────────────────────────────────────

/** Read a float dataref by path.  Returns 0.0f if not found. */
inline float GetFloat(const std::string &path)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    return ref ? XPLMGetDataf(ref) : 0.0f;
}

/** Write a float dataref by path.  No-op if not found or read-only. */
inline void SetFloat(const std::string &path, float value)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    if (ref && XPLMCanWriteDataRef(ref))
        XPLMSetDataf(ref, value);
}

/** Read a double dataref by path.  Returns 0.0 if not found. */
inline double GetDouble(const std::string &path)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    return ref ? XPLMGetDatad(ref) : 0.0;
}

/** Write a double dataref by path. */
inline void SetDouble(const std::string &path, double value)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    if (ref && XPLMCanWriteDataRef(ref))
        XPLMSetDatad(ref, value);
}

/** Read an int dataref by path.  Returns 0 if not found. */
inline int GetInt(const std::string &path)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    return ref ? XPLMGetDatai(ref) : 0;
}

/** Write an int dataref by path. */
inline void SetInt(const std::string &path, int value)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    if (ref && XPLMCanWriteDataRef(ref))
        XPLMSetDatai(ref, value);
}

/** Read a float array dataref.  Returns empty vector if not found. */
inline std::vector<float> GetFloatArray(const std::string &path, int count, int offset = 0)
{
    XPLMDataRef ref = XPLMFindDataRef(path.c_str());
    if (!ref) return {};
    std::vector<float> out(static_cast<size_t>(count));
    XPLMGetDatavf(ref, out.data(), offset, count);
    return out;
}

// ── TODO: add scenery, instance, command helpers as needed ───────────────────

} // namespace XPlaneBridge
