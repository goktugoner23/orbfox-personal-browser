#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Bottom toolbar with navigation buttons and URL bar
@interface ToolbarView : NSView <NSTextFieldDelegate>

@property (nonatomic, weak) MainWindowController* windowController;

- (void)setURL:(NSString*)url;
- (void)setCanGoBack:(BOOL)canGoBack canGoForward:(BOOL)canGoForward;
- (void)setLoading:(BOOL)isLoading;
- (void)setBookmarked:(BOOL)isBookmarked;
- (void)setBlockedCount:(int)count;
- (void)focusURLField;

// User account avatar
- (void)setSignedIn:(BOOL)signedIn;
- (void)setAvatarImage:(NSImage*)image;
- (void)setAvatarURL:(NSString*)urlString;

@end
