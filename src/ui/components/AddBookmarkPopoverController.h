#import <Cocoa/Cocoa.h>

@class SidebarView;

// Popover controller for adding a new bookmark with URL, title, nickname, description
@interface AddBookmarkPopoverController : NSViewController

@property (nonatomic, weak) SidebarView* sidebarView;

- (void)showRelativeToView:(NSView*)view withUrl:(NSString*)url title:(NSString*)title;
- (void)close;

@end
