#pragma once

#include <string>

struct ManifestConfig
{
    std::string short_name;
    int width = 800;
    int height = 600;
};

ManifestConfig ReadManifestConfig(const std::string &manifest_path);
