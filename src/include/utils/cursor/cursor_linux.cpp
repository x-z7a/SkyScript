#if LIN
#include "cursor.h"
#include <stdlib.h>
#include <string>
//#include <X11/Xlib.h>
//#include <X11/cursorfont.h>
//
//Display *x11Display = nullptr;
//Window x11Window = 0;
//Cursor cursor = 0;

void initializeCursor() {
// There seem to be issues with the X11 cursor not showing up.
// Should probably look into this, and also add support for Wayland.
// Cursor handling on linux is disabled for now.
//    x11Display = XOpenDisplay(NULL);
//    if (!x11Display) {
//        return;
//    }
//
//    x11Window = DefaultRootWindow(x11Display);
}

void destroyCursor() {
//    if (cursor) {
//        XFreeCursor(x11Display, cursor);
//    }
//    
//    if (x11Display)  {
//        XCloseDisplay(x11Display);
//    }
}

bool isCommandKeyDown() {
    return false;
}

void setCursorX11(int cursorType) {
//    if (cursor) {
//        XFreeCursor(x11Display, cursor);
//    }
//    
//    cursor = XCreateFontCursor(x11Display, cursorType);
//    XDefineCursor(x11Display, x11Window, cursor);
//    XFlush(x11Display);
}

void setCursor(CursorType cursor) {
//    bool isWayland = getenv("WAYLAND_DISPLAY");
//    if (isWayland) {
//        return;
//    }
//    
//    bool isX11 = getenv("DISPLAY") && !isWayland;
//    
//    switch (cursor) {
//        case CursorHand:
//            setCursorX11(XC_hand2);
//            break;
//            
//        case CursorText:
//            setCursorX11(XC_xterm);
//            break;
//            
//        default:
//            break;
//    }
}
#endif
