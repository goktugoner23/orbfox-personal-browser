#import "ResizeHandleView.h"
#import "MainWindowController.h"
#import "SidebarView.h"
#import "DesignSystem.h"

static const CGFloat kSidebarMinWidth = 200.0;
static const CGFloat kSidebarMaxWidth = 450.0;

@implementation ResizeHandleView {
    NSTrackingArea* _trackingArea;
    CGFloat _initialMouseX;
    CGFloat _initialSidebarWidth;
    BOOL _isDragging;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
        _isDragging = NO;
    }
    return self;
}

- (BOOL)isDragging {
    return _isDragging;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingCursorUpdate)
               owner:self
            userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)cursorUpdate:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] set];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] set];
    self.layer.backgroundColor = [DSColors surfaceHover].CGColor;
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (!_isDragging) {
        [[NSCursor arrowCursor] set];
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
}

- (void)mouseDown:(NSEvent*)event {
    _isDragging = YES;
    _initialMouseX = [self.window convertPointToScreen:event.locationInWindow].x;
    _initialSidebarWidth = _windowController.sidebarView.frame.size.width;
    self.layer.backgroundColor = [DSColors accent].CGColor;
}

- (void)mouseDragged:(NSEvent*)event {
    if (!_isDragging) return;

    CGFloat currentX = [self.window convertPointToScreen:event.locationInWindow].x;
    CGFloat deltaX = currentX - _initialMouseX;
    CGFloat newWidth = _initialSidebarWidth + deltaX;

    // Clamp to min/max
    newWidth = MAX(kSidebarMinWidth, MIN(kSidebarMaxWidth, newWidth));

    [_windowController resizeSidebarToWidth:newWidth];
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _isDragging = NO;
    self.layer.backgroundColor = [NSColor clearColor].CGColor;
    [[NSCursor arrowCursor] set];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Draw a subtle line in the center
    [[DSColors border] setFill];
    NSRect lineRect = NSMakeRect(self.bounds.size.width / 2 - 0.5, 0, 1, self.bounds.size.height);
    NSRectFill(lineRect);
}

@end
