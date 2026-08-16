#ifndef CURSOR_H
#define CURSOR_H
enum CursorType: unsigned char {
    CursorDefault = 0,
    CursorHand,
    CursorText,
    // Over the window's resize grip. Windows has exactly this cursor; AppKit
    // publishes no diagonal resize cursor, so macOS falls back to the pointing
    // hand rather than reach for a private one.
    CursorResize
};

#if APL
#ifdef __cplusplus
extern "C" {
#endif
#endif

void initializeCursor();
void setCursor(CursorType cursor);
void destroyCursor();

#if APL
#ifdef __cplusplus
}
#endif
#endif

#endif
