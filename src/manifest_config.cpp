#include "manifest_config.h"

#include <fstream>

#include "third_party/picojson.h"

ManifestConfig ReadManifestConfig(const std::string &manifest_path)
{
    ManifestConfig config;
    std::ifstream file(manifest_path);
    if (!file.is_open())
    {
        return config;
    }

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    picojson::value root;
    std::string err = picojson::parse(root, contents);
    if (!err.empty() || !root.is_object())
    {
        return config;
    }

    const auto &obj = root.get_object();
    auto it = obj.find("short_name");
    if (it != obj.end() && it->second.is_string())
    {
        config.short_name = it->second.get_string();
    }

    auto width_it = obj.find("width");
    if (width_it != obj.end() && width_it->second.is_number())
    {
        int width = static_cast<int>(width_it->second.get_number());
        if (width > 0)
        {
            config.width = width;
        }
    }

    auto height_it = obj.find("height");
    if (height_it != obj.end() && height_it->second.is_number())
    {
        int height = static_cast<int>(height_it->second.get_number());
        if (height > 0)
        {
            config.height = height;
        }
    }

    return config;
}
