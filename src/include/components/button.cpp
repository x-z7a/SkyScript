#include "button.h"

#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMUtilities.h>

#include "app.h"
#include "config.h"

Button::Button(float aWidth, float aHeight) : Image("") {
    callback = nullptr;
    relativeWidth = aWidth;
    relativeHeight = aHeight;
    pixelsWidth = App::current->viewport.width * aWidth;
    pixelsHeight = App::current->viewport.height * aHeight;
    App::current->registerButton(this);
}

Button::Button(std::string filename) : Image(filename) {
    App::current->registerButton(this);
}

void Button::destroy() {
    if (App::current) {
        App::current->unregisterButton(this);
    }
    Image::destroy();
}

bool Button::handleState(float normalizedX, float normalizedY, ButtonState state) {
    if (!visible) {
        return false;
    }

    float mouseX = App::current->viewport.width * normalizedX;
    float halfWidth = displayWidth() / 2.0f;
    float buttonX = App::current->viewport.width * x;

    if (mouseX >= (buttonX - halfWidth) && mouseX <= (buttonX + halfWidth)) {
        float mouseY = App::current->viewport.height * normalizedY;
        float halfHeight = displayHeight() / 2.0f;
        float buttonY = App::current->viewport.height * y;
        if (mouseY >= (buttonY - halfHeight) && mouseY <= (buttonY + halfHeight)) {
            if (state == kButtonClick) {
                if (callback) {
                    return callback();
                }
            }

            return true;
        }
    }

    return false;
}

void Button::setClickHandler(ButtonClickHandlerFunc cb) {
    callback = cb;
}

#if DEBUG
void Button::draw() {
    if (!debugEnabled) {
        Image::draw();
        return;
    }

    XPLMSetGraphicsState(
                         0,
                         0,
                         0,
                         0,
                         1,
                         0,
                         0);

    const auto& viewport = App::current->viewport;
    float width = displayWidth();
    float height = displayHeight();
    float x1 = viewport.x + viewport.width * x - width / 2.0f;
    float y1 = viewport.y + viewport.height * y - height / 2.0f;

    glBegin(GL_QUADS);
    glColor4f(1, 0, 0, 0.5f);

    glTexCoord2f(0, 1);
    glVertex2f(x1, y1);

    glTexCoord2f(0, 0);
    glVertex2f(x1, y1 + height);

    glTexCoord2f(1, 0);
    glVertex2f(x1 + width, y1 + height);

    glTexCoord2f(1, 1);
    glVertex2f(x1 + width, y1);

    glEnd();
}
#endif
