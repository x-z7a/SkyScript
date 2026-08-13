#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <vector>
#include <string>
#include "button.h"

enum class NotificationCorner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct NotificationOptions {
    std::string title;
    std::string body;
    NotificationCorner corner;
    float timeoutSeconds;
    float opacity;
    float slideSeconds;
    bool dismissible;
    bool playSound;
};

NotificationCorner NotificationCornerFromString(const std::string& value, NotificationCorner fallback);
std::string NotificationCornerToString(NotificationCorner corner);

class Notification {
private:
    NotificationOptions options;
    std::vector<std::string> titleLines;
    std::vector<std::string> bodyLines;
    Button *dismissButton;
    float x;
    float y;
    float width;
    float height;
    float createdAt;
    float dismissStartedAt;
    bool dismissing;
    bool finished;
    bool destroyed;
    unsigned short layoutViewportWidth;
    unsigned short layoutViewportHeight;

    void layout();
    float animationProgress() const;
    void playSound() const;

    static constexpr float panelWidth = 360.0f;
    static constexpr float horizontalTextPadding = 22.0f;
    static constexpr float topPadding = 18.0f;
    static constexpr float bottomPadding = 18.0f;
    static constexpr float titleBodyPadding = 12.0f;
    static constexpr float titleLineHeight = 17.0f;
    static constexpr float bodyLineHeight = 14.0f;
    static constexpr float margin = 18.0f;
    static constexpr float dismissButtonSize = 26.0f;
    static constexpr size_t maxTitleLines = 2;
    static constexpr size_t maxBodyLines = 6;
    
public:
    static NotificationOptions defaultOptions();

    Notification(std::string title, std::string body);
    explicit Notification(NotificationOptions options);
    ~Notification();
    void destroy();
    void dismiss();
    bool isFinished() const;
    
    void update();
    void draw();
};

#endif
