#pragma once

#import <Cocoa/Cocoa.h>

@class SidebarView;

// Pasteboard types for bookmark drag & drop
extern NSString* const kBookmarkPasteboardType;
extern NSString* const kBookmarkFolderPasteboardType;

// ============================================================================
// BOOKMARK DROP CONTAINER
// FlippedView that accepts bookmark drops for reordering and moving to folders
// ============================================================================

@interface BookmarkDropContainerView : NSView <NSDraggingDestination>
@property (nonatomic, weak) SidebarView* sidebarView;
@property (nonatomic, strong) NSView* dropIndicator;
@property (nonatomic, strong) NSView* highlightedFolderHeader;
@property (nonatomic, copy) NSString* dropTargetFolder;   // nil = root level
@property (nonatomic, assign) int dropPosition;           // Position within folder
@property (nonatomic, assign) BOOL dropOnFolderHeader;    // YES if dropping ON folder (not between items)
@end
