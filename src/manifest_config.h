#pragma once

#include <string>

/** Renderer back-end declared in manifest.json's "renderer" key. */
enum class RendererType
{
    Ultralight, ///< Default — Ultralight 1.4 CPU OSR (no CEF required)
    Cef,        ///< Chromium Embedded Framework OSR (requires SKYSCRIPT_CEF_ENABLED build)
};

struct ManifestConfig
{
    std::string  short_name;
    int          width         = 800;
    int          height        = 600;
    RendererType renderer_type = RendererType::Ultralight;
};

ManifestConfig ReadManifestConfig(const std::string &manifest_path);
