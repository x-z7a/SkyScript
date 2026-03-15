#ifndef APPSTATE_H
#define APPSTATE_H

#include <string>
#include <vector>
#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include "app.h"

class AppState {
private:
    AppState();
    ~AppState();
    static AppState* instance;
    std::string remoteVersion;

public:
    std::vector<App*> apps;
    App* activeApp;
    bool pluginInitialized = false;
    int defaultAppsStartIndex = 0;

    static AppState* getInstance();
    bool initialize();
    void deinitialize();
    void checkLatestVersion();

    void update();

    void scanApps();
    void reload();
    App* findApp(const std::string& id);
};

#endif
