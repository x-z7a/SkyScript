#if APL
#import <Cocoa/Cocoa.h>
#include "cursor.h"

extern "C" void initializeCursor() {}

extern "C" void destroyCursor() {}

extern "C" void setCursor(CursorType cursor) {
    switch (cursor) {
        case CursorHand:
            @autoreleasepool {
                [[NSCursor pointingHandCursor] set];
            }
            break;
            
        case CursorText:
            @autoreleasepool {
                [[NSCursor IBeamCursor] set];
            }
            break;

        case CursorResize:
            // AppKit's diagonal resize cursors are private
            // (_windowResizeNorthWestSouthEastCursor), and a plugin is the last
            // place to depend on private AppKit. The hand still says "this is
            // draggable", which is the part that matters.
            @autoreleasepool {
                [[NSCursor pointingHandCursor] set];
            }
            break;

        default:
            break;
    }
}

#endif
