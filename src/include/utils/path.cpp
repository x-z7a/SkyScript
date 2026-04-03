#include "path.h"
#include "config.h"
#include <XPLMPlanes.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>
#include "dataref.h"

Path* Path::instance = nullptr;

Path::Path() {
    rootDirectory = "";
    pluginDirectory = "";
    aircraftDirectory = "";
    aircraftFilename = "";
}

Path::~Path() {
    instance = nullptr;
}

Path* Path::getInstance() {
    if (instance == nullptr) {
        instance = new Path();
    }
    
    return instance;
}

void Path::reloadPaths() {
    char systemPath[512];
    XPLMGetSystemPath(systemPath);
    rootDirectory = systemPath;
    if (rootDirectory.ends_with("/")) {
        rootDirectory = rootDirectory.substr(0, rootDirectory.length() - 1);  // Remove trailing slash
    }

    char pluginFilePath[512];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, pluginFilePath, nullptr, nullptr);
    std::string pluginPath = pluginFilePath;
    // Plugin file path is like: .../Resources/plugins/PluginName/platform/Plugin.xpl
    // Go up two parent directories to get the plugin root directory
    size_t lastSep = pluginPath.find_last_of("/\\");
    if (lastSep != std::string::npos) {
        std::string platformDir = pluginPath.substr(0, lastSep);
        size_t secondLastSep = platformDir.find_last_of("/\\");
        if (secondLastSep != std::string::npos) {
            pluginDirectory = platformDir.substr(0, secondLastSep);
        } else {
            pluginDirectory = platformDir;
        }
    } else {
        pluginDirectory = rootDirectory + PLUGIN_DIRECTORY;
    }
    
    char filename[256];
    char modelPath[512];
    XPLMGetNthAircraftModel(XPLM_USER_AIRCRAFT, filename, modelPath);
    std::string aircraftExecutable = modelPath;
    if (!aircraftExecutable.empty()) {
        size_t filenamePos = aircraftExecutable.find(filename);
        if (filenamePos != std::string::npos) {
            aircraftDirectory = aircraftExecutable.substr(0, filenamePos);
        }
        else {
            aircraftDirectory = aircraftExecutable;
        }
        
        if (aircraftDirectory.ends_with("/")) {
            aircraftDirectory.pop_back();
        }
        
        aircraftFilename = filename;
    }
    else {
        aircraftDirectory = "";
        aircraftFilename = "";
    }
}

