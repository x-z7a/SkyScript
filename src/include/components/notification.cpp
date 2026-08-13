#include "notification.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <utility>

#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>
#if XPLANE_VERSION == 12
#include <XPLMSound.h>
#endif

#include "app.h"
#include "config.h"
#include "drawing.h"
#include "path.h"

namespace {
std::string normalizeCornerName(const std::string& value) {
    std::string normalized;
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            normalized.push_back(static_cast<char>(std::tolower(uc)));
        }
    }
    return normalized;
}

void trimLines(std::vector<std::string>& lines, size_t maxLines) {
    if (lines.size() <= maxLines) {
        return;
    }

    lines.resize(maxLines);
    if (!lines.empty()) {
        lines.back() += "...";
    }
}

NotificationOptions optionsWithText(std::string title, std::string body) {
    NotificationOptions options = Notification::defaultOptions();
    options.title = std::move(title);
    options.body = std::move(body);
    return options;
}
}

NotificationCorner NotificationCornerFromString(const std::string& value, NotificationCorner fallback) {
    std::string normalized = normalizeCornerName(value);
    if (normalized == "topleft" || normalized == "lefttop" || normalized == "nw") {
        return NotificationCorner::TopLeft;
    }
    if (normalized == "topright" || normalized == "righttop" || normalized == "ne") {
        return NotificationCorner::TopRight;
    }
    if (normalized == "bottomleft" || normalized == "leftbottom" || normalized == "sw") {
        return NotificationCorner::BottomLeft;
    }
    if (normalized == "bottomright" || normalized == "rightbottom" || normalized == "se") {
        return NotificationCorner::BottomRight;
    }
    return fallback;
}

std::string NotificationCornerToString(NotificationCorner corner) {
    switch (corner) {
        case NotificationCorner::TopLeft:
            return "top-left";
        case NotificationCorner::TopRight:
            return "top-right";
        case NotificationCorner::BottomLeft:
            return "bottom-left";
        case NotificationCorner::BottomRight:
            return "bottom-right";
    }

    return "top-right";
}

NotificationOptions Notification::defaultOptions() {
    return {
        "",
        "",
        NotificationCorner::TopRight,
        5.0f,
        0.78f,
        0.25f,
        true,
        true
    };
}

Notification::Notification(std::string aTitle, std::string body)
    : Notification(optionsWithText(std::move(aTitle), std::move(body))) {
}

Notification::Notification(NotificationOptions aOptions) {
    options = std::move(aOptions);
    NotificationOptions defaults = defaultOptions();
    if (options.timeoutSeconds < 0.0f) {
        options.timeoutSeconds = defaults.timeoutSeconds;
    }
    if (options.opacity <= 0.0f) {
        options.opacity = defaults.opacity;
    }
    options.opacity = std::clamp(options.opacity, 0.15f, 0.95f);
    if (options.slideSeconds <= 0.0f) {
        options.slideSeconds = defaults.slideSeconds;
    }
    options.slideSeconds = std::clamp(options.slideSeconds, 0.05f, 2.0f);

    dismissButton = nullptr;
    x = 0.0f;
    y = 0.0f;
    width = 0.0f;
    height = 0.0f;
    createdAt = XPLMGetElapsedTime();
    dismissStartedAt = 0.0f;
    dismissing = false;
    finished = false;
    destroyed = false;
    layoutViewportWidth = 0;
    layoutViewportHeight = 0;

    if (App::current && options.dismissible) {
        dismissButton = new Button(
            dismissButtonSize / App::current->viewport.width,
            dismissButtonSize / App::current->viewport.height);
        dismissButton->visible = false;
        dismissButton->setClickHandler([this]() {
            dismiss();
            return true;
        });
    }

    layout();

    if (options.playSound) {
        playSound();
    }
}

Notification::~Notification() {
    destroy();
}

void Notification::playSound() const {
#if XPLANE_VERSION == 12
    std::ifstream file(Path::getInstance()->assetsDirectory + "/notify.pcm", std::ios::binary | std::ios::ate);
    if (file) {
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        file.close();

        FMOD_CHANNEL *sound = XPLMPlayPCMOnBus(buffer.data(), (unsigned int)buffer.size(), FMOD_SOUND_FORMAT_PCM16, 22050, 1, 0, xplm_AudioInterior, nullptr, nullptr);
        if (sound) {
            XPLMSetAudioVolume(sound, 0.3f);
        }
    }
#endif
}

void Notification::destroy() {
    if (destroyed) {
        return;
    }

    destroyed = true;
    if (dismissButton) {
        dismissButton->destroy();
        delete dismissButton;
        dismissButton = nullptr;
    }
}

void Notification::dismiss() {
    if (dismissing || finished) {
        return;
    }

    dismissing = true;
    dismissStartedAt = XPLMGetElapsedTime();
    if (dismissButton) {
        dismissButton->visible = false;
    }
}

bool Notification::isFinished() const {
    return finished;
}

void Notification::update() {
    if (finished) {
        return;
    }

    float now = XPLMGetElapsedTime();
    if (!dismissing && options.timeoutSeconds > 0.0f && now - createdAt >= options.timeoutSeconds) {
        dismiss();
    }

    if (dismissing && now - dismissStartedAt >= options.slideSeconds) {
        finished = true;
    }
}

void Notification::layout() {
    App* app = App::current;
    if (!app || app->viewport.width == 0 || app->viewport.height == 0) {
        return;
    }

    if (layoutViewportWidth == app->viewport.width && layoutViewportHeight == app->viewport.height) {
        return;
    }

    layoutViewportWidth = app->viewport.width;
    layoutViewportHeight = app->viewport.height;

    width = std::clamp(panelWidth / app->viewport.width, 0.28f, 0.9f);
    float textWidth = std::max(0.1f, width - ((horizontalTextPadding * 2.0f + dismissButtonSize) / app->viewport.width));

    titleLines.clear();
    if (!options.title.empty()) {
        titleLines = Drawing::WrapWordsToLines(xplmFont_Proportional, options.title, textWidth);
        trimLines(titleLines, maxTitleLines);
    }

    bodyLines.clear();
    if (!options.body.empty()) {
        bodyLines = Drawing::WrapWordsToLines(xplmFont_Proportional, options.body, textWidth);
        trimLines(bodyLines, maxBodyLines);
    }

    float heightPixels = topPadding + bottomPadding;
    if (!titleLines.empty()) {
        heightPixels += titleLines.size() * titleLineHeight;
    }
    if (!bodyLines.empty()) {
        if (!titleLines.empty()) {
            heightPixels += titleBodyPadding;
        }
        heightPixels += bodyLines.size() * bodyLineHeight;
    }

    height = std::clamp(heightPixels / app->viewport.height, 58.0f / app->viewport.height, 0.82f);
}

float Notification::animationProgress() const {
    float duration = std::max(options.slideSeconds, 0.05f);
    float now = XPLMGetElapsedTime();
    if (dismissing) {
        return 1.0f - std::clamp((now - dismissStartedAt) / duration, 0.0f, 1.0f);
    }

    return std::clamp((now - createdAt) / duration, 0.0f, 1.0f);
}

void Notification::draw() {
    if (finished) {
        return;
    }

    layout();

    App* app = App::current;
    if (!app || width <= 0.0f || height <= 0.0f) {
        return;
    }

    float progress = animationProgress();
    if (progress <= 0.0f) {
        return;
    }

    bool rightAligned = options.corner == NotificationCorner::TopRight || options.corner == NotificationCorner::BottomRight;
    bool topAligned = options.corner == NotificationCorner::TopLeft || options.corner == NotificationCorner::TopRight;
    float marginX = margin / app->viewport.width;
    float marginY = margin / app->viewport.height;
    float targetX = rightAligned ? 1.0f - marginX - width : marginX;
    float targetY = topAligned ? 1.0f - marginY - height : marginY;
    float slideX = (width + marginX) * (1.0f - progress) * (rightAligned ? 1.0f : -1.0f);
    float slideY = (height + marginY) * (1.0f - progress) * (topAligned ? 1.0f : -1.0f);

    x = targetX + slideX;
    y = targetY + slideY;

    XPLMSetGraphicsState(
                         0,
                         0,
                         0,
                         0,
                         1,
                         0,
                         0);

    glColor4f(0.025f, 0.032f, 0.04f, options.opacity * progress);
    Drawing::DrawRoundedRect(x, y, x + width, y + height, 10.0f);

    glColor4f(0.22f, 0.46f, 0.85f, 0.75f * progress);
    Drawing::DrawRoundedRect(x, y, x + (4.0f / app->viewport.width), y + height, 3.0f);

    float textLeft = x + (horizontalTextPadding / app->viewport.width);
    float yOffset = y + height - (topPadding / app->viewport.height);
    for (const auto& titleLine : titleLines) {
        float titleWidth = Drawing::TextWidth(titleLine, 1.22f);
        Drawing::DrawText(titleLine, textLeft + titleWidth / 2.0f, yOffset, 1.22f, { 0.94f, 0.96f, 1.0f });
        yOffset -= titleLineHeight / app->viewport.height;
    }

    if (!titleLines.empty() && !bodyLines.empty()) {
        yOffset -= titleBodyPadding / app->viewport.height;
    }

    for (const auto& bodyLine : bodyLines) {
        float bodyWidth = Drawing::TextWidth(bodyLine);
        Drawing::DrawText(bodyLine, textLeft + bodyWidth / 2.0f, yOffset, 1.0f, { 0.82f, 0.86f, 0.92f });
        yOffset -= bodyLineHeight / app->viewport.height;
    }

    if (dismissButton) {
        float closeX = x + width - (horizontalTextPadding / app->viewport.width);
        float closeY = y + height - (topPadding / app->viewport.height);
        dismissButton->relativeWidth = dismissButtonSize / app->viewport.width;
        dismissButton->relativeHeight = dismissButtonSize / app->viewport.height;
        dismissButton->setPosition(closeX, closeY);
        dismissButton->visible = options.dismissible && !dismissing;
        Drawing::DrawText("x", closeX, closeY, 1.0f, { 0.76f, 0.81f, 0.88f });
    }
}
