#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// CLICKABLE VIEW
// Simple view that sends action to target on mouse click
// ============================================================================

@interface ClickableView : NSView
@property (nonatomic, weak) id target;
@property (nonatomic) SEL action;
@end

// ============================================================================
// HORIZONTAL SCROLL VIEW
// Scroll view that translates vertical scroll wheel to horizontal scrolling
// ============================================================================

@interface HorizontalScrollView : NSScrollView
@end

// ============================================================================
// FLIPPED VIEW
// NSView subclass with flipped coordinate system (top-to-bottom layout)
// ============================================================================

@interface FlippedView : NSView
@end

// ============================================================================
// TAB DROP CONTAINER VIEW
// FlippedView that accepts tab drops for reordering
// ============================================================================

@class SidebarView;

@interface TabDropContainerView : FlippedView

@property (nonatomic, weak) SidebarView* sidebarView;
@property (nonatomic, assign) int dropTargetIndex;  // -1 if not dropping

@end

// ============================================================================
// DRAGGABLE WORKSPACE TAB VIEW
// NSView that can be dragged to reorder workspaces
// ============================================================================

extern NSPasteboardType const WorkspaceTabPasteboardType;

@interface DraggableWorkspaceTabView : NSView <NSDraggingSource>

@property (nonatomic, assign) int workspaceId;
@property (nonatomic, weak) SidebarView* sidebarView;

@end

// ============================================================================
// WORKSPACE DROP CONTAINER VIEW
// Container for workspace tabs that accepts drops for reordering
// ============================================================================

@interface WorkspaceDropContainerView : NSView

@property (nonatomic, weak) SidebarView* sidebarView;
@property (nonatomic, assign) int dropTargetIndex;

@end
