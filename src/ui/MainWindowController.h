#pragma once

#import <Cocoa/Cocoa.h>

#include "tab_manager.h"
#include "browser_client.h"
#include "include/cef_download_handler.h"
#import "GestureContainerView.h"

@class SidebarView;
@class ToolbarView;
@class FindBarView;

// Main window controller for the browser
@interface MainWindowController : NSWindowController <NSWindowDelegate, GestureContainerDelegate>

@property (nonatomic, readonly) TabManager* tabManager;
@property (nonatomic, readonly) SidebarView* sidebarView;
@property (nonatomic, readonly) ToolbarView* toolbarView;
@property (nonatomic, readonly) NSView* browserContainer;

- (instancetype)initWithTabManager:(TabManager*)tabManager;

// Tab operations
- (void)createNewTab:(NSString*)url;
- (void)createBackgroundTab:(NSString*)url;
- (void)closeTab:(int)tabId;
- (void)closeCurrentTab;
- (void)activateTab:(int)tabId;
- (void)reopenClosedTab;
- (void)openLinkInNewTab:(NSString*)url background:(BOOL)background;
- (void)openSettingsInNewTab;

// Reusable browser actions (for context menus, middle-click, etc.)
- (void)openUrlInCurrentTab:(NSString*)url;
- (void)openUrlInNewTab:(NSString*)url;
- (void)openUrlInBackgroundTab:(NSString*)url;
- (void)copyUrlToClipboard:(NSString*)url;

// Navigation
- (void)navigateToURL:(NSString*)url;
- (void)goBack;
- (void)goForward;
- (void)reload;
- (void)stopLoading;

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
- (void)bookmarkThisPage;
- (void)newBookmarkFolder;

// Account
- (void)showAccountPopover:(NSView*)anchorView;

// History
- (void)reloadHistoryPanelIfVisible;

// Fullscreen
- (void)setFullscreen:(BOOL)fullscreen;

// Downloads
- (void)showDownloadDialogForFile:(NSString*)filename
                             size:(int64_t)totalBytes
                         callback:(CefRefPtr<CefBeforeDownloadCallback>)callback;

// Find in Page
- (void)showFindBar;
- (void)hideFindBar;

// DevTools
- (void)toggleDevTools;
- (void)showDevToolsAtPoint:(int)x y:(int)y;
- (void)closeDevTools;
@property (nonatomic, readonly) BOOL devToolsOpen;

@end
