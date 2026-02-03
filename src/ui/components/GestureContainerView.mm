#import "GestureContainerView.h"

@implementation GestureContainerView {
    BOOL _isTrackingGesture;
    NSPoint _gestureStartPoint;
    NSPoint _gestureCurrentPoint;

    // For L-shape detection: track if we've moved down significantly
    BOOL _hasMovedDown;
    CGFloat _maxDownwardDistance;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        _gesturesEnabled = YES;
        _minimumGestureDistance = 50.0;
        _isTrackingGesture = NO;
        _hasMovedDown = NO;
        _maxDownwardDistance = 0;
    }
    return self;
}

- (void)rightMouseDown:(NSEvent*)event {
    if (!_gesturesEnabled) {
        [super rightMouseDown:event];
        return;
    }

    _isTrackingGesture = YES;
    _gestureStartPoint = [self convertPoint:event.locationInWindow fromView:nil];
    _gestureCurrentPoint = _gestureStartPoint;
    _hasMovedDown = NO;
    _maxDownwardDistance = 0;

    // Don't call super - we're handling this
}

- (void)rightMouseDragged:(NSEvent*)event {
    if (!_isTrackingGesture) {
        [super rightMouseDragged:event];
        return;
    }

    _gestureCurrentPoint = [self convertPoint:event.locationInWindow fromView:nil];

    // Track downward movement for L-shape detection
    // Note: In Cocoa, Y increases upward, so downward movement means currentY < startY
    CGFloat downwardDistance = _gestureStartPoint.y - _gestureCurrentPoint.y;
    if (downwardDistance > _maxDownwardDistance) {
        _maxDownwardDistance = downwardDistance;
    }

    // Consider it "moved down" if we've gone down at least 30pt
    if (_maxDownwardDistance >= 30.0) {
        _hasMovedDown = YES;
    }
}

- (void)rightMouseUp:(NSEvent*)event {
    if (!_isTrackingGesture) {
        [super rightMouseUp:event];
        return;
    }

    _isTrackingGesture = NO;

    NSPoint endPoint = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat dx = endPoint.x - _gestureStartPoint.x;
    CGFloat dy = endPoint.y - _gestureStartPoint.y;  // Positive = up, negative = down

    CGFloat horizontalDistance = fabs(dx);
    CGFloat verticalDistance = fabs(dy);

    GestureType recognizedGesture = GestureTypeNone;

    // Check for L-shape gesture: down then right
    // We need: moved down significantly at some point, AND ended up to the right
    if (_hasMovedDown && dx >= _minimumGestureDistance) {
        // L-shape: went down, then right
        recognizedGesture = GestureTypeLShape;
    }
    // Check for horizontal gestures (must be primarily horizontal)
    else if (horizontalDistance >= _minimumGestureDistance && horizontalDistance > verticalDistance * 1.5) {
        if (dx < 0) {
            recognizedGesture = GestureTypeLeft;  // Dragged left → Back
        } else {
            recognizedGesture = GestureTypeRight;  // Dragged right → Forward
        }
    }

    if (recognizedGesture != GestureTypeNone) {
        if ([_gestureDelegate respondsToSelector:@selector(gestureRecognized:)]) {
            [_gestureDelegate gestureRecognized:recognizedGesture];
        }
    } else {
        // No gesture recognized - show context menu as normal
        // We need to manually trigger the context menu since we intercepted rightMouseDown
        NSMenu* menu = [self menuForEvent:event];
        if (menu) {
            [NSMenu popUpContextMenu:menu withEvent:event forView:self];
        }
    }
}

// Forward mouse events to subviews (the CEF browser views)
- (NSView*)hitTest:(NSPoint)point {
    // For left clicks and other events, let subviews handle them normally
    // Right-click events are handled by our rightMouse* methods
    return [super hitTest:point];
}

// Allow subviews to receive mouse events
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}

@end
