#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <functional>
#include <XPLMDisplay.h>
#include "cursor.h"

class Browser;
class Notification;

#include "button.h"

enum class AppType { Folder, Web, Default };

struct WindowViewport {
    int x;
    int y;
    unsigned short width;
    unsigned short height;
    unsigned short textureWidth;
    unsigned short textureHeight;
    unsigned short browserWidth;
    unsigned short browserHeight;
    static constexpr unsigned char bytesPerPixel = 4;
};

struct AppConfiguration {
    std::string homepage;
    bool audio_muted;
    unsigned short minimum_width;
    unsigned char scroll_speed;
    std::string forced_language;
    std::string user_agent;
    bool hide_addressbar;
    unsigned char framerate;
#if DEBUG
    float debug_value_1;
    float debug_value_2;
    float debug_value_3;
#endif
};

typedef std::function<void()> CallbackFunc;

struct DelayedTask {
    CallbackFunc func;
    float executeAfterElapsedSeconds;
};

class App {
private:
    std::vector<DelayedTask> tasks;
    std::vector<Button *> buttons;
    Notification *notification;
    bool setViewport(int x, int y, unsigned short width, unsigned short height);

public:
    static App* current;

    static constexpr unsigned short defaultWindowWidth = 1024;
    static constexpr unsigned short defaultWindowHeight = 768;
    static constexpr float browserTopRatio = 1.0f;
    static constexpr float toolbarY = 0.985f;

    std::string name;
    std::string id;
    AppType type;

    XPLMWindowID window;
    float brightness;
    WindowViewport viewport;
    AppConfiguration config;
    bool visible;
    Browser *browser;
    CursorType activeCursor;

    App(const std::string& name, const std::string& id, AppType type, const AppConfiguration& config);
    ~App();

    void initialize();
    void deinitialize();
    void registerWindow();

    void update();
    void draw();
    bool syncWindowGeometry(bool resizeBrowser = true);
    bool normalizeWindowPoint(int x, int y, float *normalizedX, float *normalizedY);
    void applyWindowMode();

    bool updateButtons(float normalizedX, float normalizedY, ButtonState state);
    void registerButton(Button *button);
    void unregisterButton(Button *button);

    void showBrowser(std::string url = "");
    void hideBrowser();
    void showNotification(Notification *notification);
    void executeDelayed(CallbackFunc func, float delaySeconds);

    static AppConfiguration defaultConfig();
    static AppConfiguration parseManifest(const std::string& manifestPath, const AppConfiguration& defaults);
};

#endif
