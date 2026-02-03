#import "GestureContainerView.h"
#include "browser_client.h"

@implementation GestureContainerView {
    BOOL _isTrackingGesture;
    NSPoint _gestureStartPoint;
    NSPoint _gestureCurrentPoint;
    BOOL _didDrag;  // Track if mouse actually moved during gesture

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
        _didDrag = NO;
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
    _didDrag = NO;

    // Suppress CEF context menu during gesture tracking
    SuppressContextMenu(true);

    // Don't call super - we're handling this
}

- (void)rightMouseDragged:(NSEvent*)event {
    if (!_isTrackingGesture) {
        [super rightMouseDragged:event];
        return;
    }

    _gestureCurrentPoint = [self convertPoint:event.locationInWindow fromView:nil];

    // Track if we've actually moved (more than a few pixels to account for jitter)
    CGFloat totalDistance = hypot(_gestureCurrentPoint.x - _gestureStartPoint.x,
                                   _gestureCurrentPoint.y - _gestureStartPoint.y);
    if (totalDistance > 5.0) {
        _didDrag = YES;
    }

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

    // Re-enable context menu
    SuppressContextMenu(false);

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
    } else if (!_didDrag) {
        // No drag occurred - this was just a right-click, forward to browser view
        // Find the actual browser view (should be a subview of this container)
        for (NSView* subview in self.subviews) {
            if (!subview.hidden) {
                // Forward both down and up events - CEF needs both to show context menu
                NSEvent* downEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                                        location:event.locationInWindow
                                                   modifierFlags:event.modifierFlags
                                                       timestamp:event.timestamp
                                                    windowNumber:event.windowNumber
                                                         context:nil
                                                     eventNumber:event.eventNumber
                                                      clickCount:1
                                                        pressure:event.pressure];
                [subview rightMouseDown:downEvent];

                // Also send the mouse up event
                NSEvent* upEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp
                                                      location:event.locationInWindow
                                                 modifierFlags:event.modifierFlags
                                                     timestamp:event.timestamp
                                                  windowNumber:event.windowNumber
                                                       context:nil
                                                   eventNumber:event.eventNumber
                                                    clickCount:1
                                                      pressure:event.pressure];
                [subview rightMouseUp:upEvent];
                break;
            }
        }
    }
    // If drag occurred but no gesture recognized, do nothing (user was just exploring)
}

// Intercept right-clicks for gesture detection
- (NSView*)hitTest:(NSPoint)point {
    // First check if the point is even in our bounds
    NSPoint localPoint = [self convertPoint:point fromView:self.superview];
    if (!NSPointInRect(localPoint, self.bounds)) {
        return nil;
    }

    if (!_gesturesEnabled) {
        return [super hitTest:point];
    }

    // Check if the current event is a right-click
    NSEvent* currentEvent = [NSApp currentEvent];
    if (currentEvent &&
        (currentEvent.type == NSEventTypeRightMouseDown ||
         currentEvent.type == NSEventTypeRightMouseDragged ||
         currentEvent.type == NSEventTypeRightMouseUp)) {
        // Return self to intercept right-click events for gesture detection
        return self;
    }

    // For all other events, let subviews handle them normally
    return [super hitTest:point];
}

// Allow subviews to receive mouse events
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}

// Prevent automatic context menu during gesture tracking
- (NSMenu*)menuForEvent:(NSEvent*)event {
    // We handle context menus manually after gesture detection
    // Return nil to prevent automatic context menu
    if (_gesturesEnabled) {
        return nil;
    }
    return [super menuForEvent:event];
}

@end
