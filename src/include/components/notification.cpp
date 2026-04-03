#include "notification.h"

#include <cmath>
#include <fstream>

#include <XPLMGraphics.h>
#include <XPLMUtilities.h>
#if XPLANE_VERSION == 12
#include <XPLMSound.h>
#endif

#include "app.h"
#include "config.h"
#include "drawing.h"
#include "path.h"

Notification::Notification(std::string aTitle, std::string body) {
    dismissButton = nullptr;
    x = 0.0f;
    y = 0.0f;
    width = 0.0f;
    height = 0.0f;
    animateIn = 0.0f;

    title = aTitle;
    dismissButton = new Button(0.3f, 0.05f);
    dismissButton->setClickHandler([]() {
        if (App::current) {
            App::current->showNotification(nullptr);
        }
        return true;
    });

    x = 0.5f;
    y = 0.55f;

    width = 0.3f;
    x -= width / 2.0f;
    bodyLines = Drawing::WrapWordsToLines(xplmFont_Proportional, body, width - ((horizontalTextPadding * 2.0f) / App::current->viewport.width));
    height = (topPadding + titleBodyPadding + (bodyLines.size() * bodyLineHeight) + buttonPadding + dismissButton->displayHeight() + buttonPadding) / App::current->viewport.height;

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
    if (dismissButton) {
        dismissButton->destroy();
    }
}

void Notification::update() {
}

void Notification::draw() {
    if (animateIn < 1.0f) {
        animateIn = fmin(animateIn + 0.1f, 1.0f);
    }

    XPLMSetGraphicsState(
                         0,
                         0,
                         0,
                         0,
                         1,
                         0,
                         0);

    App* app = App::current;

    glColor4f(0.0f, 0.0f, 0.0f, animateIn * 0.6f);
    Drawing::DrawRect(0.0f, 0.0f, 1.0f, 1.0f);

    set_brightness(app->brightness);
    Drawing::DrawRoundedRect(x, y - (height / 2.0f), x + width, y + (height / 2.0f), 16.0f);

    float yOffset = y + (height / 2.0f) - (topPadding / app->viewport.height);
    Drawing::DrawText(title, x + (width / 2.0f), yOffset, 1.4f);

    yOffset -= titleBodyPadding / app->viewport.height;
    for (const auto& bodyLine : bodyLines) {
        Drawing::DrawText(bodyLine, x + (width / 2.0f), yOffset);
        yOffset -= bodyLineHeight / app->viewport.height;
    }

    set_brightness(app->brightness * 0.2f);
    Drawing::DrawLine(x, yOffset, x + width, yOffset, 1.0f);
    yOffset -= (buttonPadding / app->viewport.height) * 3.0f;

    dismissButton->setPosition(0.5f, yOffset);
    Drawing::DrawText("OK", x + (width / 2.0f), yOffset, 1.4f, { app->brightness * 0.4f, app->brightness * 0.4f, app->brightness * 1.0f });
}
