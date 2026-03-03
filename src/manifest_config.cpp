#include "manifest_config.h"

#include <fstream>

#include "third_party/picojson.h"

ManifestConfig ReadManifestConfig(const std::string &manifest_path)
{
    ManifestConfig config;
    std::ifstream file(manifest_path);
    if (!file.is_open())
        return config;

    std::string contents((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value root;
    std::string err = picojson::parse(root, contents);
    if (!err.empty() || !root.is_object())
        return config;

    const auto &obj = root.get_object();

    auto it = obj.find("short_name");
    if (it != obj.end() && it->second.is_string())
        config.short_name = it->second.get_string();

    auto width_it = obj.find("width");
    if (width_it != obj.end() && width_it->second.is_number())
    {
        int w = static_cast<int>(width_it->second.get_number());
        if (w > 0) config.width = w;
    }

    auto height_it = obj.find("height");
    if (height_it != obj.end() && height_it->second.is_number())
    {
        int h = static_cast<int>(height_it->second.get_number());
        if (h > 0) config.height = h;
    }

    auto renderer_it = obj.find("renderer");
    if (renderer_it != obj.end() && renderer_it->second.is_string())
    {
        const std::string &r = renderer_it->second.get_string();
        if (r == "cef")
            config.renderer_type = RendererType::Cef;
        // anything else (including "ultralight") keeps the default
    }

    return config;
}
