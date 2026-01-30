#import "SidebarHelperViews.h"
#import "TabRowView.h"
#import "SidebarView.h"
#import "MainWindowController.h"
#include "tab_manager.h"

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

// ============================================================================
// TAB DROP CONTAINER VIEW
// ============================================================================

static const CGFloat kTabRowHeight = 36.0;
static const CGFloat kDropIndicatorHeight = 2.0;

@implementation TabDropContainerView {
    NSView* _dropIndicator;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _dropTargetIndex = -1;
        [self registerForDraggedTypes:@[TabRowPasteboardType]];

        // Create drop indicator (horizontal line)
        _dropIndicator = [[NSView alloc] initWithFrame:NSMakeRect(4, 0, frame.size.width - 8, kDropIndicatorHeight)];
        _dropIndicator.wantsLayer = YES;
        _dropIndicator.layer.backgroundColor = [NSColor systemBlueColor].CGColor;
        _dropIndicator.layer.cornerRadius = 1;
        _dropIndicator.hidden = YES;
        [self addSubview:_dropIndicator];
    }
    return self;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if ([[sender draggingPasteboard] availableTypeFromArray:@[TabRowPasteboardType]]) {
        return NSDragOperationMove;
    }
    return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint location = [self convertPoint:[sender draggingLocation] fromView:nil];

    // Find the target index based on Y position
    int newIndex = [self indexForDropAtPoint:location];
    if (newIndex != _dropTargetIndex) {
        _dropTargetIndex = newIndex;
        [self updateDropIndicator];
    }

    return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropTargetIndex = -1;
    _dropIndicator.hidden = YES;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard* pb = [sender draggingPasteboard];
    NSString* tabIdStr = [pb stringForType:TabRowPasteboardType];

    if (!tabIdStr || _dropTargetIndex < 0) {
        return NO;
    }

    int tabId = [tabIdStr intValue];
    TabManager* tabManager = _sidebarView.windowController.tabManager;

    if (tabManager && tabManager->ReorderTab(tabId, _dropTargetIndex)) {
        [_sidebarView reloadTabs];
        return YES;
    }

    return NO;
}

- (void)draggingEnded:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropTargetIndex = -1;
    _dropIndicator.hidden = YES;
}

- (int)indexForDropAtPoint:(NSPoint)point {
    // Count visible tab rows (excluding drop indicator)
    int count = 0;
    for (NSView* subview in self.subviews) {
        if ([subview isKindOfClass:[TabRowView class]]) {
            count++;
        }
    }

    // Find insertion point based on Y position
    int index = (int)(point.y / kTabRowHeight);
    if (index < 0) index = 0;
    if (index > count) index = count;
    return index;
}

- (void)updateDropIndicator {
    if (_dropTargetIndex < 0) {
        _dropIndicator.hidden = YES;
        return;
    }

    _dropIndicator.hidden = NO;
    CGFloat y = _dropTargetIndex * kTabRowHeight;
    NSRect frame = _dropIndicator.frame;
    frame.origin.y = y - kDropIndicatorHeight / 2;
    frame.size.width = self.bounds.size.width - 8;
    _dropIndicator.frame = frame;

    // Bring to front
    [_dropIndicator removeFromSuperview];
    [self addSubview:_dropIndicator];
}

@end

// ============================================================================
// DRAGGABLE WORKSPACE TAB VIEW
// ============================================================================

NSPasteboardType const WorkspaceTabPasteboardType = @"com.orbfox.workspacetab";

@implementation DraggableWorkspaceTabView {
    NSPoint _dragStartPoint;
    BOOL _isDragging;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _isDragging = NO;
    }
    return self;
}

- (void)mouseDown:(NSEvent*)event {
    _dragStartPoint = [self convertPoint:event.locationInWindow fromView:nil];
    _isDragging = NO;
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _isDragging = NO;
}

- (void)mouseDragged:(NSEvent*)event {
    NSPoint currentPoint = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat dx = currentPoint.x - _dragStartPoint.x;
    CGFloat dy = currentPoint.y - _dragStartPoint.y;
    CGFloat distance = sqrt(dx * dx + dy * dy);

    if (!_isDragging && distance > 5) {
        _isDragging = YES;
        [self startDragWithEvent:event];
    }
}

- (void)startDragWithEvent:(NSEvent*)event {
    NSPasteboardItem* pbItem = [[NSPasteboardItem alloc] init];
    [pbItem setString:[NSString stringWithFormat:@"%d", _workspaceId] forType:WorkspaceTabPasteboardType];

    // Create snapshot for drag image
    NSImage* dragImage = [[NSImage alloc] initWithSize:self.bounds.size];
    [dragImage lockFocus];
    [[NSColor colorWithWhite:0.3 alpha:0.8] setFill];
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:6 yRadius:6];
    [path fill];
    [dragImage unlockFocus];

    NSDraggingItem* dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:pbItem];
    [dragItem setDraggingFrame:self.bounds contents:dragImage];

    [self beginDraggingSessionWithItems:@[dragItem] event:event source:self];
}

- (NSDragOperation)draggingSession:(NSDraggingSession*)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    (void)session;
    (void)context;
    return NSDragOperationMove;
}

@end

// ============================================================================
// WORKSPACE DROP CONTAINER VIEW
// ============================================================================

@implementation WorkspaceDropContainerView {
    NSView* _dropIndicator;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _dropTargetIndex = -1;
        [self registerForDraggedTypes:@[WorkspaceTabPasteboardType]];

        // Vertical drop indicator
        _dropIndicator = [[NSView alloc] initWithFrame:NSMakeRect(0, 4, 2, frame.size.height - 8)];
        _dropIndicator.wantsLayer = YES;
        _dropIndicator.layer.backgroundColor = [NSColor systemBlueColor].CGColor;
        _dropIndicator.layer.cornerRadius = 1;
        _dropIndicator.hidden = YES;
        [self addSubview:_dropIndicator];
    }
    return self;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if ([[sender draggingPasteboard] availableTypeFromArray:@[WorkspaceTabPasteboardType]]) {
        return NSDragOperationMove;
    }
    return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint location = [self convertPoint:[sender draggingLocation] fromView:nil];

    int newIndex = [self indexForDropAtPoint:location];
    if (newIndex != _dropTargetIndex) {
        _dropTargetIndex = newIndex;
        [self updateDropIndicator];
    }

    return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropTargetIndex = -1;
    _dropIndicator.hidden = YES;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard* pb = [sender draggingPasteboard];
    NSString* wsIdStr = [pb stringForType:WorkspaceTabPasteboardType];

    if (!wsIdStr || _dropTargetIndex < 0) {
        return NO;
    }

    int workspaceId = [wsIdStr intValue];
    TabManager* tabManager = _sidebarView.windowController.tabManager;

    if (tabManager && tabManager->ReorderWorkspace(workspaceId, _dropTargetIndex)) {
        [_sidebarView reloadWorkspaceTabs];
        return YES;
    }

    return NO;
}

- (void)draggingEnded:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropTargetIndex = -1;
    _dropIndicator.hidden = YES;
}

- (int)indexForDropAtPoint:(NSPoint)point {
    // Find workspace tabs (DraggableWorkspaceTabView instances)
    NSMutableArray<NSView*>* tabs = [NSMutableArray array];
    for (NSView* subview in self.subviews) {
        if ([subview isKindOfClass:[DraggableWorkspaceTabView class]]) {
            [tabs addObject:subview];
        }
    }

    // Sort by X position
    [tabs sortUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        return a.frame.origin.x < b.frame.origin.x ? NSOrderedAscending : NSOrderedDescending;
    }];

    // Find insertion index
    int index = 0;
    for (NSView* tab in tabs) {
        CGFloat midX = tab.frame.origin.x + tab.frame.size.width / 2;
        if (point.x < midX) {
            return index;
        }
        index++;
    }
    return index;
}

- (void)updateDropIndicator {
    if (_dropTargetIndex < 0) {
        _dropIndicator.hidden = YES;
        return;
    }

    // Find workspace tabs
    NSMutableArray<NSView*>* tabs = [NSMutableArray array];
    for (NSView* subview in self.subviews) {
        if ([subview isKindOfClass:[DraggableWorkspaceTabView class]]) {
            [tabs addObject:subview];
        }
    }

    [tabs sortUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        return a.frame.origin.x < b.frame.origin.x ? NSOrderedAscending : NSOrderedDescending;
    }];

    CGFloat x = 4;  // Default left position
    if (_dropTargetIndex < (int)tabs.count && tabs.count > 0) {
        NSView* tab = tabs[_dropTargetIndex];
        x = tab.frame.origin.x - 2;
    } else if (tabs.count > 0) {
        NSView* lastTab = tabs.lastObject;
        x = lastTab.frame.origin.x + lastTab.frame.size.width + 2;
    }

    _dropIndicator.hidden = NO;
    NSRect frame = _dropIndicator.frame;
    frame.origin.x = x;
    _dropIndicator.frame = frame;

    // Bring to front
    [_dropIndicator removeFromSuperview];
    [self addSubview:_dropIndicator];
}

@end
