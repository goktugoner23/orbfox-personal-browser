#import "SidebarHelperViews.h"

// ============================================================================
// CLICKABLE VIEW
// ============================================================================

@implementation ClickableView {
    NSInteger _clickableTag;
}

- (void)setTag:(NSInteger)tag {
    _clickableTag = tag;
}

- (NSInteger)tag {
    return _clickableTag;
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    if (_target && _action && [_target respondsToSelector:_action]) {
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [_target performSelector:_action withObject:self];
        #pragma clang diagnostic pop
    }
}

@end

// ============================================================================
// HORIZONTAL SCROLL VIEW
// ============================================================================

@implementation HorizontalScrollView

- (void)scrollWheel:(NSEvent*)event {
    NSClipView* clipView = self.contentView;
    NSPoint currentOrigin = clipView.bounds.origin;
    NSRect docBounds = self.documentView.bounds;

    // Use horizontal delta directly (two-finger horizontal swipe)
    // Also convert vertical scroll to horizontal for scroll wheels
    CGFloat delta = event.deltaX;
    if (delta == 0 && event.deltaY != 0) {
        delta = event.deltaY;  // Convert vertical to horizontal
    }

    if (delta != 0) {
        // Swipe right (negative delta) = scroll left (decrease X)
        CGFloat newX = currentOrigin.x - delta * 8.0;

        // Clamp to valid range
        CGFloat maxX = MAX(0, docBounds.size.width - clipView.bounds.size.width);
        newX = MAX(0, MIN(newX, maxX));

        [clipView scrollToPoint:NSMakePoint(newX, 0)];
        [self reflectScrolledClipView:clipView];
    }
    // Don't call super - block vertical scrolling
}

// Allow clicks to pass through to subviews
- (NSView*)hitTest:(NSPoint)point {
    NSView* hit = [super hitTest:point];
    return hit;
}

@end

// ============================================================================
// FLIPPED VIEW
// ============================================================================

@implementation FlippedView

- (BOOL)isFlipped {
    return YES;
}

@end
