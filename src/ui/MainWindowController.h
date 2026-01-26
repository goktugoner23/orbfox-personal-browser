#pragma once

#import <Cocoa/Cocoa.h>

#include "tab_manager.h"
#include "browser_client.h"

@class SidebarView;
@class ToolbarView;

// Main window controller for the browser
@interface MainWindowController : NSWindowController <NSWindowDelegate>

@property (nonatomic, readonly) TabManager* tabManager;
@property (nonatomic, readonly) SidebarView* sidebarView;
@property (nonatomic, readonly) ToolbarView* toolbarView;
@property (nonatomic, readonly) NSView* browserContainer;

- (instancetype)initWithTabManager:(TabManager*)tabManager;

// Tab operations
- (void)createNewTab:(NSString*)url;
- (void)closeTab:(int)tabId;
- (void)closeCurrentTab;
- (void)activateTab:(int)tabId;

// Navigation
- (void)navigateToURL:(NSString*)url;
- (void)goBack;
- (void)goForward;
- (void)reload;

// Update UI
- (void)updateURLBar:(NSString*)url;
- (void)updateNavigationButtons:(BOOL)canGoBack canGoForward:(BOOL)canGoForward;

// Sidebar
- (void)toggleSidebarCollapse:(BOOL)collapse;
- (void)resizeSidebarToWidth:(CGFloat)newWidth;
- (void)focusURLBar;

// Panel actions (for menu bar)
- (void)showTabsPanel;
- (void)showHistoryPanel;
- (void)showBookmarksPanel;
- (void)showDownloadsPanel;

// Bookmarks
- (void)reloadBookmarksPanel;
- (void)updateBookmarkState;

@end
