#ifndef SKYSCRIPT_H
#define SKYSCRIPT_H

#include "include/app.h"

#include <string>
#include <vector>

namespace SkyScript {

// Initialize the SkyScript library and register the flight loop. Call from XPluginStart.
void initialize();

// Shutdown the SkyScript library and unregister the flight loop. Call from XPluginStop.
void shutdown();

// Scan the apps/ directory for manifest files and create windows for each.
// Returns true if apps were discovered and initialized.
bool loadAppsFromDirectory();

// Tear down all apps and rescan the apps/ directory.
void reloadApps();

// Create a new browser window with the given name, id, and configuration.
// The window is created hidden; call app->showBrowser() to display it.
App* createAppWindow(const std::string& name, const std::string& id, const AppConfiguration& config = App::defaultConfig());

// Destroy a browser window and free its resources.
void destroyAppWindow(App* app);

// Get all currently managed app windows.
const std::vector<App*>& getAppWindows();

// Get/set the active (most recently shown) app window.
App* getActiveApp();
void setActiveApp(App* app);

// Find an app by its id, or nullptr if not found.
App* findApp(const std::string& id);

// Destroy all managed windows and their dataref bindings.
void destroyAllAppWindows();

// Set the log prefix used in debug messages (e.g. "[MyPlugin]").
void setLogPrefix(const std::string& prefix);

}

#endif
