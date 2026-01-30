#import <Cocoa/Cocoa.h>

@class SidebarView;

typedef NS_ENUM(NSInteger, BookmarkPopoverMode) {
    BookmarkPopoverModeAdd,
    BookmarkPopoverModeEdit
};

// Reusable popover controller for adding/editing bookmarks
@interface AddBookmarkPopoverController : NSViewController

@property (nonatomic, weak) SidebarView* sidebarView;
@property (nonatomic, assign, readonly) BookmarkPopoverMode mode;

// Show popover for adding a new bookmark
- (void)showRelativeToView:(NSView*)view withUrl:(NSString*)url title:(NSString*)title;

// Show popover for editing an existing bookmark
- (void)showEditRelativeToView:(NSView*)view bookmarkId:(int64_t)bookmarkId;

// Show as modal sheet (for when no anchor view is available)
- (void)showAsSheetInWindow:(NSWindow*)window withUrl:(NSString*)url title:(NSString*)title;
- (void)showEditAsSheetInWindow:(NSWindow*)window bookmarkId:(int64_t)bookmarkId;

- (void)close;

@end
