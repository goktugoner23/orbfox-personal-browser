#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Popover for editing bookmark name and folder (Vivaldi-style)
@interface BookmarkEditPopoverController : NSViewController

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, copy) void (^onDismiss)(void);

- (void)showRelativeToView:(NSView*)view
               forBookmark:(int64_t)bookmarkId
                     title:(NSString*)title
                       url:(NSString*)url
                    folder:(NSString*)folder;

- (void)close;

@end
