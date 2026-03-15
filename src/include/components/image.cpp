#include "image.h"

#include <fstream>

#include <XPLMGraphics.h>
#include <XPLMUtilities.h>

#include "app.h"
#include "config.h"
#include "lodepng.h"
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

Image::Image(std::string filename) {
    x = 0.0f;
    y = 0.0f;
    rotationDegrees = 0;
    visible = true;
    textureId = 0;
    relativeWidth = 0.0f;
    relativeHeight = 0.0f;
    pixelsWidth = 0;
    pixelsHeight = 0;

    if (filename.empty()) {
        return;
    }

    unsigned char *data = nullptr;
    unsigned int error = 1;
    std::ifstream fileExistsHandle(filename);
    if (fileExistsHandle.good()) {
        if (filename.ends_with(".png") || filename.ends_with(".PNG")) {
            error = lodepng_decode32_file(&data, &pixelsWidth, &pixelsHeight, filename.c_str());
        }
        else if (filename.ends_with(".svg") || filename.ends_with(".SVG")) {
            NSVGimage *image = nsvgParseFromFile(filename.c_str(), "px", 96);
            struct NSVGrasterizer *rast = nsvgCreateRasterizer();
            pixelsWidth = 16;
            pixelsHeight = 16;
            data = (unsigned char *)malloc(pixelsWidth * pixelsHeight * 4);
            nsvgRasterize(rast, image, 0, 0, (float)pixelsWidth / image->width, data, pixelsWidth, pixelsHeight, pixelsWidth * 4);
            for (unsigned int i = 0; i < pixelsWidth * pixelsHeight * 4; i++) {
                if (data[i] != 0) {
                    error = 0;
                    break;
                }
            }
        }
    }

    if (error) {
        debug("Could not load image (code %i): %s\n", error, filename.c_str());
        return;
    }

    if (data) {
        XPLMGenerateTextureNumbers(&textureId, 1);
        XPLMBindTexture2d(textureId, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixelsWidth, pixelsHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        free(data);
    }

#if SCALE_IMAGES
    float aspectRatio = (float)pixelsWidth / pixelsHeight;
    pixelsWidth = (pixelsWidth * (float)App::current->viewport.width) / 800.0f;
    pixelsHeight = pixelsWidth / aspectRatio;
#endif

    relativeWidth = pixelsWidth / (float)App::current->viewport.width;
    relativeHeight = pixelsHeight / (float)App::current->viewport.height;
}

void Image::destroy() {
    if (textureId) {
        XPLMBindTexture2d(textureId, 0);
        glDeleteTextures(1, (GLuint *)&textureId);
        textureId = 0;
    }
}

void Image::draw(unsigned short aRotationDegrees) {
    rotationDegrees = aRotationDegrees;
    draw();
}

void Image::draw() {
    if (!textureId || !visible) {
        return;
    }

    XPLMSetGraphicsState(
                         0,
                         1,
                         0,
                         0,
                         1,
                         0,
                         0);

    XPLMBindTexture2d(textureId, 0);

    const auto& viewport = App::current->viewport;
    float centerX = viewport.x + viewport.width * x;
    float centerY = viewport.y + viewport.height * y;
    float width = displayWidth();
    float height = displayHeight();

    if (rotationDegrees > 0) {
        glPushMatrix();
        glTranslatef(centerX, centerY, 0.0f);
        glRotatef(rotationDegrees, 0.0f, 0.0f, -1.0f);
        glTranslatef(-centerX, -centerY, 0.0f);
    }

    float x1 = centerX - width / 2.0f;
    float y1 = centerY - height / 2.0f;

    glBegin(GL_QUADS);
    set_brightness(App::current->brightness);

    glTexCoord2f(0, 1);
    glVertex2f(x1, y1);

    glTexCoord2f(0, 0);
    glVertex2f(x1, y1 + height);

    glTexCoord2f(1, 0);
    glVertex2f(x1 + width, y1 + height);

    glTexCoord2f(1, 1);
    glVertex2f(x1 + width, y1);

    glEnd();

    if (rotationDegrees > 0) {
        glPopMatrix();
    }
}

void Image::setPosition(float normalizedX, float normalizedY, unsigned short aRotationDegrees) {
    x = normalizedX;
    y = normalizedY;
    rotationDegrees = aRotationDegrees;
}

float Image::displayWidth() const {
    return relativeWidth > 0.0f ? App::current->viewport.width * relativeWidth : pixelsWidth;
}

float Image::displayHeight() const {
    return relativeHeight > 0.0f ? App::current->viewport.height * relativeHeight : pixelsHeight;
}
