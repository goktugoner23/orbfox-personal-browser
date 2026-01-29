#import "DevToolsDividerView.h"
#import "DesignSystem.h"

@implementation DevToolsDividerView {
    NSTrackingArea* _trackingArea;
    BOOL _isDragging;
}

- (BOOL)isDragging {
    return _isDragging;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) [self removeTrackingArea:_trackingArea];
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
                                                  owner:self
                                               userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Subtle divider line
    [[DSColors divider] setFill];
    NSRectFill(self.bounds);

    // Grip indicator: three small dots vertically centered
    NSColor* gripColor = [DSColors dividerGrip];
    [gripColor setFill];
    CGFloat cx = NSMidX(self.bounds);
    CGFloat cy = NSMidY(self.bounds);
    CGFloat dotSize = 2.0;
    CGFloat dotSpacing = 5.0;
    for (int i = -1; i <= 1; i++) {
        NSRect dot = NSMakeRect(cx - dotSize / 2, cy + i * dotSpacing - dotSize / 2, dotSize, dotSize);
        [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
    }
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] push];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (!_isDragging) [NSCursor pop];
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    _isDragging = YES;
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _isDragging = NO;
    [NSCursor pop];
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

@end
