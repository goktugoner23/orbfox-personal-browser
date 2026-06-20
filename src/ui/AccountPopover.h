#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Account popover for sign-in and sync settings
@interface AccountPopoverController : NSViewController

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, copy) void (^onSignIn)(void);
@property (nonatomic, copy) void (^onSignOut)(void);
@property (nonatomic, copy) void (^onSyncNow)(void);

// Show the popover relative to a view (avatar button)
- (void)showRelativeToView:(NSView*)view;

// Update display after auth state changes
- (void)updateAuthState;

// Close the popover
- (void)close;

@end
