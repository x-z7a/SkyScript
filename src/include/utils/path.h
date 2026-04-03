#ifndef PATH_H
#define PATH_H

#include <string>

class Path {
private:
    Path();
    ~Path();
    static Path* instance;

public:
    std::string rootDirectory;
    std::string pluginDirectory;
    std::string assetsDirectory;
    std::string aircraftDirectory;
    std::string aircraftFilename;
    
    static Path* getInstance();
    void reloadPaths();
    void setPluginPath(const std::string& path);
    void setAssetsPath(const std::string& path);
};

#endif
