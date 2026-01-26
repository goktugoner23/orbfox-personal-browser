#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Bottom toolbar with navigation buttons and URL bar
@interface ToolbarView : NSView <NSTextFieldDelegate>

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, strong) NSButton* backButton;
@property (nonatomic, strong) NSButton* forwardButton;
@property (nonatomic, strong) NSButton* reloadButton;
@property (nonatomic, strong) NSTextField* urlField;

- (void)setURL:(NSString*)url;
- (void)setCanGoBack:(BOOL)canGoBack canGoForward:(BOOL)canGoForward;
- (void)setLoading:(BOOL)isLoading;
- (void)focusURLField;

@end
