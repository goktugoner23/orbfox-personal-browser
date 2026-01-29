#pragma once

#import <Cocoa/Cocoa.h>

@class SidebarView;

// ============================================================================
// DOWNLOAD ROW VIEW
// Row view with right-click context menu, selection, and double-click support
// for download items in the sidebar
// ============================================================================

@interface DownloadRowView : NSView

@property (nonatomic, assign) uint32_t downloadId;
@property (nonatomic, copy) NSString* downloadPath;
@property (nonatomic, copy) NSString* downloadUrl;
@property (nonatomic, assign) BOOL isInProgress;
@property (nonatomic, assign) BOOL isStopped;
@property (nonatomic, assign) BOOL isComplete;
@property (nonatomic, assign) BOOL isFileMissing;  // File was downloaded but deleted from disk
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, weak) SidebarView* sidebarView;

@end
