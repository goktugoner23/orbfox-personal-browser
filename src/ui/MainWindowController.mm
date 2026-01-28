#import "MainWindowController.h"
#import "SidebarView.h"
#import "ToolbarView.h"
#import "FindBarView.h"
#import "Components.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "history_storage.h"
#include "bookmark_storage.h"
#include "download_manager.h"

// Extern functions to access global storage
extern HistoryStorage* GetHistoryStorage();
extern BookmarkStorage* GetBookmarkStorage();

static const CGFloat kSidebarDefaultWidth = 280.0;
static const CGFloat kSidebarMinWidth = 200.0;
static const CGFloat kSidebarMaxWidth = 450.0;
static const CGFloat kToolbarHeight = 44.0;
static const CGFloat kResizeHandleWidth = 6.0;

// ============================================================================
// RESIZE HANDLE VIEW
// Draggable divider for resizing sidebar
// ============================================================================

@interface ResizeHandleView : NSView
@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, assign) CGFloat initialMouseX;
@property (nonatomic, assign) CGFloat initialSidebarWidth;
@property (nonatomic, assign) BOOL isDragging;
@end

@implementation ResizeHandleView {
    NSTrackingArea* _trackingArea;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
        _isDragging = NO;
    }
    return self;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingCursorUpdate)
               owner:self
            userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)cursorUpdate:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] set];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] set];
    self.layer.backgroundColor = [DSColors surfaceHover].CGColor;
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (!_isDragging) {
        [[NSCursor arrowCursor] set];
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
}

- (void)mouseDown:(NSEvent*)event {
    _isDragging = YES;
    _initialMouseX = [self.window convertPointToScreen:event.locationInWindow].x;
    _initialSidebarWidth = _windowController.sidebarView.frame.size.width;
    self.layer.backgroundColor = [DSColors accent].CGColor;
}

- (void)mouseDragged:(NSEvent*)event {
    if (!_isDragging) return;

    CGFloat currentX = [self.window convertPointToScreen:event.locationInWindow].x;
    CGFloat deltaX = currentX - _initialMouseX;
    CGFloat newWidth = _initialSidebarWidth + deltaX;

    // Clamp to min/max
    newWidth = MAX(kSidebarMinWidth, MIN(kSidebarMaxWidth, newWidth));

    [_windowController resizeSidebarToWidth:newWidth];
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _isDragging = NO;
    self.layer.backgroundColor = [NSColor clearColor].CGColor;
    [[NSCursor arrowCursor] set];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Draw a subtle line in the center
    [[DSColors border] setFill];
    NSRect lineRect = NSMakeRect(self.bounds.size.width / 2 - 0.5, 0, 1, self.bounds.size.height);
    NSRectFill(lineRect);
}

@end

@interface MainWindowController ()
@property (nonatomic, readwrite) TabManager* tabManager;
@property (nonatomic, readwrite) SidebarView* sidebarView;
@property (nonatomic, readwrite) ToolbarView* toolbarView;
@property (nonatomic, readwrite) NSView* browserContainer;
@property (nonatomic, strong) ResizeHandleView* resizeHandle;
@property (nonatomic, assign) CGFloat currentSidebarWidth;
@property (nonatomic, strong) FindBarView* findBar;
@property (nonatomic, copy) NSString* lastSearchText;
@end

@implementation MainWindowController

- (instancetype)initWithTabManager:(TabManager*)tabManager {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 1280, 800);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                   NSWindowStyleMaskClosable |
                                   NSWindowStyleMaskMiniaturizable |
                                   NSWindowStyleMaskResizable;

    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:styleMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];

    self = [super initWithWindow:window];
    if (self) {
        _tabManager = tabManager;
        _currentSidebarWidth = kSidebarDefaultWidth;

        window.delegate = self;
        window.minSize = NSMakeSize(800, 600);
        window.title = @"Personal Browser";
        window.titlebarAppearsTransparent = YES;
        window.titleVisibility = NSWindowTitleHidden;
        window.backgroundColor = [DSColors background];

        // Full-size content
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;

        [self setupViews];
        [self setupCallbacks];
        [self setupKeyboardShortcuts];

        [window center];
    }
    return self;
}

- (void)setupViews {
    NSView* contentView = self.window.contentView;
    NSRect bounds = contentView.bounds;

    // Account for title bar
    CGFloat titleBarHeight = 28;
    CGFloat contentHeight = bounds.size.height - titleBarHeight;

    // Sidebar - extends to bottom of window
    _sidebarView = [[SidebarView alloc] initWithFrame:NSMakeRect(0, 0, _currentSidebarWidth, contentHeight)];
    _sidebarView.windowController = self;
    _sidebarView.autoresizingMask = NSViewHeightSizable;
    [contentView addSubview:_sidebarView];

    // Resize handle - draggable divider
    _resizeHandle = [[ResizeHandleView alloc] initWithFrame:NSMakeRect(
        _currentSidebarWidth - kResizeHandleWidth / 2, 0, kResizeHandleWidth, contentHeight)];
    _resizeHandle.windowController = self;
    _resizeHandle.autoresizingMask = NSViewHeightSizable;
    [contentView addSubview:_resizeHandle];

    // Bottom toolbar
    _toolbarView = [[ToolbarView alloc] initWithFrame:NSMakeRect(
        _currentSidebarWidth, 0, bounds.size.width - _currentSidebarWidth, kToolbarHeight)];
    _toolbarView.windowController = self;
    _toolbarView.autoresizingMask = NSViewWidthSizable;
    [contentView addSubview:_toolbarView];

    // Browser container
    CGFloat browserX = _currentSidebarWidth;
    CGFloat browserY = kToolbarHeight;
    CGFloat browserWidth = bounds.size.width - _currentSidebarWidth;
    CGFloat browserHeight = contentHeight - kToolbarHeight;

    _browserContainer = [[NSView alloc] initWithFrame:NSMakeRect(browserX, browserY, browserWidth, browserHeight)];
    _browserContainer.wantsLayer = YES;
    _browserContainer.layer.backgroundColor = [NSColor blackColor].CGColor;
    _browserContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [contentView addSubview:_browserContainer];
}

- (void)setupCallbacks {
    __weak MainWindowController* weakSelf = self;

    TabManagerCallbacks callbacks;

    callbacks.on_tab_created = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf createBrowserForTab:tab];
            [strongSelf.sidebarView reloadTabs];
        });
    };

    callbacks.on_tab_closed = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // IMPORTANT: Capture browser reference BEFORE dispatch_async!
        // The Tab* will be deleted immediately after this callback returns,
        // so we must capture the ref-counted CefRefPtr here.
        CefRefPtr<CefBrowser> browser = tab->browser;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf removeBrowserView:browser];
            [strongSelf.sidebarView reloadTabs];
        });
    };

    callbacks.on_tab_activated = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // Capture all data BEFORE dispatch_async to avoid dangling pointer
        int tabId = tab->id;
        std::string url = tab->url;
        CefRefPtr<CefBrowser> browser = tab->browser;
        int blockedCount = tab->client ? tab->client->GetBlockedCount() : 0;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf showBrowserWithRef:browser];
            [strongSelf.sidebarView selectTab:tabId];
            [strongSelf.toolbarView setURL:[NSString stringWithUTF8String:url.c_str()]];
            [strongSelf.toolbarView setBlockedCount:blockedCount];
        });
    };

    callbacks.on_tab_updated = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // Capture all data BEFORE dispatch_async to avoid dangling pointer
        int tabId = tab->id;
        std::string title = tab->title;
        std::string url = tab->url;
        bool isLoading = tab->is_loading;
        std::vector<unsigned char> faviconData = tab->favicon_data;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.sidebarView updateTab:tabId
                                         title:[NSString stringWithUTF8String:title.c_str()]
                                     isLoading:isLoading];

            // Update favicon if we have data
            if (!faviconData.empty()) {
                NSData* nsData = [NSData dataWithBytes:faviconData.data()
                                                length:faviconData.size()];
                [strongSelf.sidebarView updateTab:tabId faviconData:nsData];
            }

            // Update window title and URL bar if this is the active tab
            Tab* activeTab = strongSelf.tabManager->GetActiveTab();
            if (activeTab && activeTab->id == tabId) {
                strongSelf.window.title = [NSString stringWithUTF8String:title.c_str()];
                [strongSelf.toolbarView setURL:[NSString stringWithUTF8String:url.c_str()]];
            }
        });
    };

    callbacks.on_workspace_changed = [weakSelf](Workspace* workspace) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !workspace) return;

        // Capture workspace name before async
        std::string name = workspace->name;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.sidebarView updateWorkspaceButton];
            [strongSelf.sidebarView reloadTabs];
        });
    };

    _tabManager->SetCallbacks(callbacks);
}

- (void)createBrowserForTab:(Tab*)tab {
    if (!tab) return;

    // Create browser client for this tab
    CefRefPtr<BrowserClient> client = new BrowserClient();
    tab->client = client;

    // Set up callbacks for this browser
    __weak MainWindowController* weakSelf = self;
    int tabId = tab->id;

    // When browser is created, store reference and configure view
    client->SetBrowserCreatedCallback([weakSelf, tabId](CefRefPtr<CefBrowser> browser) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        Tab* tab = strongSelf.tabManager->GetTabById(tabId);
        if (tab) {
            tab->browser = browser;

            dispatch_async(dispatch_get_main_queue(), ^{
                // Configure the browser view
                CefRefPtr<CefBrowserHost> host = browser->GetHost();
                if (host) {
                    NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
                    if (browserView) {
                        browserView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

                        // Show this browser if it's the active tab
                        if (strongSelf.tabManager->GetActiveTab() == tab) {
                            [strongSelf showBrowserForTab:tab];
                        }
                    }
                }
            });
        }
    });

    client->SetTitleChangeCallback([weakSelf, tabId](const std::string& title) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf && strongSelf.tabManager) {
            strongSelf.tabManager->UpdateTabTitle(tabId, title);
        }
    });

    client->SetAddressChangeCallback([weakSelf, tabId](const std::string& url) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf && strongSelf.tabManager) {
            strongSelf.tabManager->UpdateTabUrl(tabId, url);

            dispatch_async(dispatch_get_main_queue(), ^{
                if (strongSelf.tabManager->GetActiveTab() &&
                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                    [strongSelf.toolbarView setURL:[NSString stringWithUTF8String:url.c_str()]];
                }
            });
        }
    });

    client->SetLoadingStateCallback([weakSelf, tabId](bool isLoading, bool canGoBack, bool canGoForward) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf && strongSelf.tabManager) {
            strongSelf.tabManager->UpdateTabLoadingState(tabId, isLoading);

            // Record history when page finishes loading
            BOOL historyAdded = NO;
            if (!isLoading) {
                Tab* tab = strongSelf.tabManager->GetTabById(tabId);
                if (tab && !tab->url.empty() && tab->url.find("data:") != 0) {
                    HistoryStorage* history = GetHistoryStorage();
                    if (history) {
                        history->AddEntry(tab->url, tab->title);
                        historyAdded = YES;
                    }
                }
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                if (strongSelf.tabManager->GetActiveTab() &&
                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                    [strongSelf.toolbarView setCanGoBack:canGoBack canGoForward:canGoForward];
                    [strongSelf.toolbarView setLoading:isLoading];
                }

                // Refresh history panel if visible and history was just added
                if (historyAdded) {
                    [strongSelf reloadHistoryPanelIfVisible];
                }
            });
        }
    });

    // Handle popup requests (open as new tab)
    client->SetPopupRequestCallback([weakSelf](const std::string& url) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf createNewTab:[NSString stringWithUTF8String:url.c_str()]];
            });
        }
    });

    // Handle favicon changes
    client->SetFaviconChangeCallback([weakSelf, tabId](const std::string& url, const std::vector<unsigned char>& png_data) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf && strongSelf.tabManager) {
            strongSelf.tabManager->UpdateTabFavicon(tabId, url, png_data);
        }
    });

    // Handle download dialog
    client->SetDownloadDialogCallback([weakSelf](const std::string& suggested_name,
                                                  int64_t total_bytes,
                                                  CefRefPtr<CefBeforeDownloadCallback> callback) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        // Get filename - use suggested_name or generate one
        NSString* filename = nil;
        if (!suggested_name.empty()) {
            filename = [NSString stringWithUTF8String:suggested_name.c_str()];
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf showDownloadDialogForFile:filename
                                             size:total_bytes
                                         callback:callback];
        });
    });

    // Handle blocked tracker count updates
    client->SetBlockedCountCallback([weakSelf, tabId](int blockedCount) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            // Only update toolbar if this is the active tab
            Tab* activeTab = strongSelf.tabManager->GetActiveTab();
            if (activeTab && activeTab->id == tabId) {
                [strongSelf.toolbarView setBlockedCount:blockedCount];
            }
        });
    });

    // Handle fullscreen mode changes (HTML5 Fullscreen API)
    client->SetFullscreenChangeCallback([weakSelf, tabId](bool fullscreen) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            // Only handle fullscreen if this is the active tab
            Tab* activeTab = strongSelf.tabManager->GetActiveTab();
            if (activeTab && activeTab->id == tabId) {
                [strongSelf setFullscreen:fullscreen];
            }
        });
    });

    // Create browser settings
    CefBrowserSettings settings;

    // Initial URL
    std::string url = tab->url.empty() ? "https://www.google.com" : tab->url;

    // Window info - embed in our browser container
    CefWindowInfo window_info;
    NSRect bounds = _browserContainer.bounds;
    CefRect cef_rect(0, 0, bounds.size.width, bounds.size.height);
    window_info.SetAsChild((__bridge void*)_browserContainer, cef_rect);

    // Create browser asynchronously
    CefBrowserHost::CreateBrowser(window_info, client, url, settings, nullptr, nullptr);
}

- (void)removeBrowserForTab:(Tab*)tab {
    if (!tab || !tab->browser) return;

    CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
    if (host) {
        NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
        if (browserView) {
            [browserView removeFromSuperview];
        }
        host->CloseBrowser(true);
    }
    tab->browser = nullptr;
}

- (void)removeBrowserView:(CefRefPtr<CefBrowser>)browser {
    if (!browser) return;

    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (host) {
        NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
        if (browserView) {
            [browserView removeFromSuperview];
        }
        host->CloseBrowser(true);
    }
}

- (void)showBrowserWithRef:(CefRefPtr<CefBrowser>)browser {
    // Hide all browser views
    for (NSView* subview in _browserContainer.subviews) {
        subview.hidden = YES;
    }

    // Show the specified browser view
    if (browser) {
        CefRefPtr<CefBrowserHost> host = browser->GetHost();
        if (host) {
            NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
            if (browserView) {
                browserView.hidden = NO;
                browserView.frame = _browserContainer.bounds;
            }
        }
    }
}

- (void)showBrowserForTab:(Tab*)tab {
    // Hide all browser views
    for (NSView* subview in _browserContainer.subviews) {
        subview.hidden = YES;
    }

    // Show the active tab's browser view
    if (tab && tab->browser) {
        CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
        if (host) {
            NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
            if (browserView) {
                browserView.hidden = NO;
                browserView.frame = _browserContainer.bounds;
            }
        }
    }
}

#pragma mark - Tab Operations

- (void)createNewTab:(NSString*)url {
    std::string urlStr = url ? [url UTF8String] : "";
    _tabManager->CreateTab(urlStr);
}

- (void)closeTab:(int)tabId {
    _tabManager->CloseTab(tabId);

    // If no tabs left, create a new one
    Workspace* workspace = _tabManager->GetActiveWorkspace();
    if (workspace && workspace->tabs.empty()) {
        _tabManager->CreateTab("");
    }
}

- (void)closeCurrentTab {
    Tab* tab = _tabManager->GetActiveTab();
    if (tab) {
        [self closeTab:tab->id];
    }
}

- (void)activateTab:(int)tabId {
    _tabManager->SetActiveTab(tabId);
}

#pragma mark - Navigation

- (void)navigateToURL:(NSString*)url {
    Tab* tab = _tabManager->GetActiveTab();
    if (!tab || !tab->browser) return;

    NSString* urlToLoad = url;

    // Add https:// if no scheme specified
    if (![url hasPrefix:@"http://"] && ![url hasPrefix:@"https://"] && ![url hasPrefix:@"file://"]) {
        // Check if it looks like a URL or a search query
        if ([url containsString:@"."] && ![url containsString:@" "]) {
            urlToLoad = [@"https://" stringByAppendingString:url];
        } else {
            // Search query
            NSString* encoded = [url stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLQueryAllowedCharacterSet]];
            urlToLoad = [NSString stringWithFormat:@"https://www.google.com/search?q=%@", encoded];
        }
    }

    tab->browser->GetMainFrame()->LoadURL([urlToLoad UTF8String]);
}

- (void)goBack {
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->browser && tab->browser->CanGoBack()) {
        tab->browser->GoBack();
    }
}

- (void)goForward {
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->browser && tab->browser->CanGoForward()) {
        tab->browser->GoForward();
    }
}

- (void)reload {
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->browser) {
        tab->browser->Reload();
    }
}

- (void)stopLoading {
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->browser) {
        tab->browser->StopLoad();
    }
}

- (void)setFullscreen:(BOOL)fullscreen {
    NSWindow* window = self.window;
    BOOL isCurrentlyFullscreen = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;

    if (fullscreen && !isCurrentlyFullscreen) {
        // Enter fullscreen
        [window toggleFullScreen:nil];
    } else if (!fullscreen && isCurrentlyFullscreen) {
        // Exit fullscreen
        [window toggleFullScreen:nil];
    }
}

#pragma mark - UI Updates

- (void)updateURLBar:(NSString*)url {
    [_toolbarView setURL:url];
    [self updateBookmarkState];
}

- (void)updateNavigationButtons:(BOOL)canGoBack canGoForward:(BOOL)canGoForward {
    [_toolbarView setCanGoBack:canGoBack canGoForward:canGoForward];
}

- (void)focusURLBar {
    [_toolbarView focusURLField];
}

#pragma mark - Sidebar

static const CGFloat kIconStripWidth = 44.0;

- (void)resizeSidebarToWidth:(CGFloat)newWidth {
    // Ensure we're not collapsed
    if (_sidebarView.isCollapsed) return;

    _currentSidebarWidth = newWidth;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;

    // Update sidebar frame
    NSRect sidebarFrame = _sidebarView.frame;
    sidebarFrame.size.width = newWidth;
    _sidebarView.frame = sidebarFrame;

    // Update resize handle position
    NSRect handleFrame = _resizeHandle.frame;
    handleFrame.origin.x = newWidth - kResizeHandleWidth / 2;
    _resizeHandle.frame = handleFrame;

    // Update toolbar position and width
    NSRect toolbarFrame = _toolbarView.frame;
    toolbarFrame.origin.x = newWidth;
    toolbarFrame.size.width = contentView.bounds.size.width - newWidth;
    _toolbarView.frame = toolbarFrame;

    // Update browser container position and width
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    _browserContainer.frame = NSMakeRect(newWidth, kToolbarHeight,
                                          contentView.bounds.size.width - newWidth, browserHeight);

    // Update sidebar internal layout
    [_sidebarView updateLayoutForWidth:newWidth];

    // Resize visible browser views to fit new container
    for (NSView* subview in _browserContainer.subviews) {
        if (!subview.hidden) {
            subview.frame = _browserContainer.bounds;
        }
    }
}

- (void)toggleSidebarCollapse:(BOOL)collapse {
    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;

    CGFloat newSidebarWidth = collapse ? kIconStripWidth : _currentSidebarWidth;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = 0.2;
        context.allowsImplicitAnimation = YES;

        // Animate sidebar width
        NSRect sidebarFrame = _sidebarView.frame;
        sidebarFrame.size.width = newSidebarWidth;
        _sidebarView.animator.frame = sidebarFrame;

        // Hide/show resize handle
        _resizeHandle.animator.alphaValue = collapse ? 0.0 : 1.0;

        // Animate resize handle position
        NSRect handleFrame = _resizeHandle.frame;
        handleFrame.origin.x = newSidebarWidth - kResizeHandleWidth / 2;
        _resizeHandle.animator.frame = handleFrame;

        // Animate toolbar position and width
        NSRect toolbarFrame = _toolbarView.frame;
        toolbarFrame.origin.x = newSidebarWidth;
        toolbarFrame.size.width = contentView.bounds.size.width - newSidebarWidth;
        _toolbarView.animator.frame = toolbarFrame;

        // Animate browser container position and width
        CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
        NSRect browserFrame = NSMakeRect(newSidebarWidth, kToolbarHeight,
                                          contentView.bounds.size.width - newSidebarWidth, browserHeight);
        _browserContainer.animator.frame = browserFrame;

    } completionHandler:^{
        // Resize browser views to fit new container
        for (NSView* subview in self->_browserContainer.subviews) {
            if (!subview.hidden) {
                subview.frame = self->_browserContainer.bounds;
            }
        }

        // Disable resize handle when collapsed
        self->_resizeHandle.hidden = collapse;
    }];
}

#pragma mark - Panel Actions

- (void)showTabsPanel {
    [_sidebarView showPanel:SidebarPanelTabs];
}

- (void)showHistoryPanel {
    [_sidebarView showPanel:SidebarPanelHistory];
}

- (void)showBookmarksPanel {
    [_sidebarView showPanel:SidebarPanelFavorites];
}

- (void)showDownloadsPanel {
    [_sidebarView showPanel:SidebarPanelDownloads];
}

#pragma mark - Bookmarks

- (void)reloadBookmarksPanel {
    [_sidebarView reloadBookmarks];
}

- (void)updateBookmarkState {
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    Tab* activeTab = _tabManager->GetActiveTab();
    if (!activeTab) {
        [_toolbarView setBookmarked:NO];
        return;
    }

    BOOL isBookmarked = bookmarks->IsBookmarked(activeTab->url);
    [_toolbarView setBookmarked:isBookmarked];
}

- (void)bookmarkThisPage {
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    Tab* activeTab = _tabManager->GetActiveTab();
    if (!activeTab) return;

    // Toggle bookmark - remove if exists, add if not
    if (bookmarks->IsBookmarked(activeTab->url)) {
        bookmarks->DeleteBookmarkByUrl(activeTab->url);
    } else {
        bookmarks->AddBookmark(activeTab->url, activeTab->title);
    }

    [_sidebarView reloadBookmarks];
    [self updateBookmarkState];
}

- (void)newBookmarkFolder {
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    // Get default folder name
    int nextNum = bookmarks->GetNextFolderNumber();
    NSString* defaultName = [NSString stringWithFormat:@"Collection %d", nextNum];

    // Show modal dialog for folder name
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"New Folder";
    alert.informativeText = @"Enter a name for the new bookmark folder:";
    [alert addButtonWithTitle:@"Create"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 240, 24)];
    input.stringValue = defaultName;
    input.placeholderString = @"Folder name";
    alert.accessoryView = input;

    [alert.window makeFirstResponder:input];

    NSModalResponse response = [alert runModal];

    if (response == NSAlertFirstButtonReturn) {
        NSString* folderName = [input.stringValue stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];

        if (folderName.length == 0) {
            folderName = defaultName;
        }

        // Check if folder already exists
        if (bookmarks->FolderExists([folderName UTF8String])) {
            NSAlert* errorAlert = [[NSAlert alloc] init];
            errorAlert.messageText = @"Folder Exists";
            errorAlert.informativeText = [NSString stringWithFormat:
                @"A folder named \"%@\" already exists.", folderName];
            errorAlert.alertStyle = NSAlertStyleWarning;
            [errorAlert addButtonWithTitle:@"OK"];
            [errorAlert runModal];
            return;
        }

        // Create the folder
        bookmarks->CreateFolder([folderName UTF8String]);
        [_sidebarView reloadBookmarks];

        // Show bookmarks panel to see the new folder
        [_sidebarView showPanel:SidebarPanelFavorites];
    }
}

#pragma mark - History

- (void)reloadHistoryPanelIfVisible {
    if (_sidebarView.activePanel == SidebarPanelHistory) {
        [_sidebarView reloadHistory];
    }
}

#pragma mark - Downloads

- (NSString*)formatBytesForDialog:(int64_t)bytes {
    if (bytes < 0) {
        return @"Unknown size";
    } else if (bytes < 1024) {
        return [NSString stringWithFormat:@"%lld bytes", bytes];
    } else if (bytes < 1024 * 1024) {
        return [NSString stringWithFormat:@"%.1f KB", bytes / 1024.0];
    } else if (bytes < 1024 * 1024 * 1024) {
        return [NSString stringWithFormat:@"%.1f MB", bytes / (1024.0 * 1024.0)];
    } else {
        return [NSString stringWithFormat:@"%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0)];
    }
}

- (void)showDownloadDialogForFile:(NSString*)filename
                             size:(int64_t)totalBytes
                         callback:(CefRefPtr<CefBeforeDownloadCallback>)callback {
    // Ensure we have a valid filename
    NSString* safeName = filename;
    if (!safeName || safeName.length == 0) {
        safeName = @"download";
    }

    // Check if this is a restart (resuming a stopped download)
    bool isRestart = DownloadManager::GetInstance().GetAndClearIsRestart();
    if (isRestart) {
        // For restarts, use saved preference if available
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        NSString* lastChoice = [defaults stringForKey:@"DownloadAction"];

        if ([lastChoice isEqualToString:@"save"]) {
            [self saveDownloadToDefaultLocation:safeName callback:callback];
            return;
        } else if ([lastChoice isEqualToString:@"saveAs"]) {
            [self showSavePanelForFile:safeName callback:callback rememberChoice:NO];
            return;
        }
        // No saved preference, fall through to show dialog
    }

    // Show the dialog for new downloads or when no preference saved
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Download File";

    NSString* sizeStr = (totalBytes > 0) ? [self formatBytesForDialog:totalBytes] : @"Unknown";
    alert.informativeText = [NSString stringWithFormat:@"File: %@\nSize: %@\n\nWhere would you like to save this file?",
                             safeName, sizeStr];

    // Add buttons (in order: Save, Save As..., Cancel)
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Save As..."];
    [alert addButtonWithTitle:@"Cancel"];

    // Set button key equivalents
    alert.buttons[0].keyEquivalent = @"\r";  // Return key for Save
    alert.buttons[2].keyEquivalent = @"\033";  // Escape for Cancel

    // Show as sheet
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            // Remember choice for restarts
            [[NSUserDefaults standardUserDefaults] setObject:@"save" forKey:@"DownloadAction"];
            [self saveDownloadToDefaultLocation:safeName callback:callback];

        } else if (response == NSAlertSecondButtonReturn) {
            // Remember choice for restarts
            [[NSUserDefaults standardUserDefaults] setObject:@"saveAs" forKey:@"DownloadAction"];
            [self showSavePanelForFile:safeName callback:callback rememberChoice:NO];
        }
        // Cancel - don't call Continue, download is canceled
    }];
}

- (void)saveDownloadToDefaultLocation:(NSString*)filename callback:(CefRefPtr<CefBeforeDownloadCallback>)callback {
    NSString* downloadsPath = [NSHomeDirectory() stringByAppendingPathComponent:@"Downloads"];
    NSString* fullPath = [downloadsPath stringByAppendingPathComponent:filename];

    // Check if file exists and add number suffix if needed
    NSFileManager* fm = [NSFileManager defaultManager];
    if ([fm fileExistsAtPath:fullPath]) {
        NSString* baseName = [filename stringByDeletingPathExtension];
        NSString* ext = [filename pathExtension];
        int counter = 1;
        do {
            NSString* newName;
            if (ext.length > 0) {
                newName = [NSString stringWithFormat:@"%@ (%d).%@", baseName, counter, ext];
            } else {
                newName = [NSString stringWithFormat:@"%@ (%d)", baseName, counter];
            }
            fullPath = [downloadsPath stringByAppendingPathComponent:newName];
            counter++;
        } while ([fm fileExistsAtPath:fullPath]);
    }

    callback->Continue([fullPath UTF8String], false);
}

- (void)showSavePanelForFile:(NSString*)filename
                    callback:(CefRefPtr<CefBeforeDownloadCallback>)callback
              rememberChoice:(BOOL)remember {
    (void)remember;
    NSSavePanel* savePanel = [NSSavePanel savePanel];
    savePanel.nameFieldStringValue = filename;
    savePanel.directoryURL = [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Downloads"]];

    [savePanel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse panelResponse) {
        if (panelResponse == NSModalResponseOK && savePanel.URL) {
            callback->Continue([savePanel.URL.path UTF8String], false);
        }
        // If canceled, don't call Continue - download is canceled
    }];
}

#pragma mark - Keyboard Shortcuts

- (void)setupKeyboardShortcuts {
    // Monitor for keyboard events
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event) {
        if (event.modifierFlags & NSEventModifierFlagCommand) {
            NSString* chars = event.charactersIgnoringModifiers;
            if ([chars isEqualToString:@"t"]) {
                // Cmd+T: New tab
                [self createNewTab:@""];
                return nil;
            } else if ([chars isEqualToString:@"w"]) {
                // Cmd+W: Close current tab
                Tab* tab = self.tabManager->GetActiveTab();
                if (tab) {
                    [self closeTab:tab->id];
                }
                return nil;
            } else if ([chars isEqualToString:@"l"]) {
                // Cmd+L: Focus URL bar
                [self focusURLBar];
                return nil;
            } else if ([chars isEqualToString:@"f"]) {
                // Cmd+F: Find in page
                [self showFindBar];
                return nil;
            } else if (chars.length == 1) {
                // Cmd+1-9: Switch to tab by index
                unichar c = [chars characterAtIndex:0];
                if (c >= '1' && c <= '9') {
                    int index = c - '1';  // 0-based index
                    Workspace* workspace = self.tabManager->GetActiveWorkspace();
                    if (workspace && index < static_cast<int>(workspace->tabs.size())) {
                        [self activateTab:workspace->tabs[index]->id];
                    }
                    return nil;
                }
            }
        }
        return event;
    }];
}

#pragma mark - NSWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    // Close all browsers
    for (const auto& workspace : _tabManager->GetWorkspaces()) {
        for (const auto& tab : workspace->tabs) {
            if (tab->browser) {
                tab->browser->GetHost()->CloseBrowser(true);
            }
        }
    }
    return YES;
}

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    CefQuitMessageLoop();
}

#pragma mark - Find in Page

- (void)showFindBar {
    if (!_findBar) {
        CGFloat barWidth = 300;
        CGFloat barHeight = 36;
        CGFloat rightMargin = 10;
        CGFloat topMargin = 10;

        NSRect browserFrame = _browserContainer.frame;
        CGFloat x = browserFrame.origin.x + browserFrame.size.width - barWidth - rightMargin;
        CGFloat y = browserFrame.origin.y + browserFrame.size.height - barHeight - topMargin;

        _findBar = [[FindBarView alloc] initWithFrame:NSMakeRect(x, y, barWidth, barHeight)];
        _findBar.windowController = self;
        _findBar.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;

        __weak MainWindowController* weakSelf = self;

        _findBar.onSearchChanged = ^(NSString* text) {
            MainWindowController* strongSelf = weakSelf;
            if (!strongSelf) return;

            Tab* tab = strongSelf.tabManager->GetActiveTab();
            if (tab && tab->browser) {
                if (text.length > 0) {
                    bool isNewSearch = ![text isEqualToString:strongSelf.lastSearchText];
                    strongSelf.lastSearchText = text;
                    tab->browser->GetHost()->Find([text UTF8String], true, false, !isNewSearch);
                } else {
                    strongSelf.lastSearchText = nil;
                    tab->browser->GetHost()->StopFinding(true);
                    [strongSelf.findBar updateMatchCount:0 activeMatch:0];
                }
            }
        };

        _findBar.onNext = ^{
            MainWindowController* strongSelf = weakSelf;
            if (!strongSelf || !strongSelf.findBar.searchText.length) return;

            Tab* tab = strongSelf.tabManager->GetActiveTab();
            if (tab && tab->browser) {
                tab->browser->GetHost()->Find([strongSelf.findBar.searchText UTF8String], true, false, true);
            }
        };

        _findBar.onPrev = ^{
            MainWindowController* strongSelf = weakSelf;
            if (!strongSelf || !strongSelf.findBar.searchText.length) return;

            Tab* tab = strongSelf.tabManager->GetActiveTab();
            if (tab && tab->browser) {
                tab->browser->GetHost()->Find([strongSelf.findBar.searchText UTF8String], false, false, true);
            }
        };

        _findBar.onClose = ^{
            [weakSelf hideFindBar];
        };
    }

    // Set up find result callback on active tab
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->client) {
        __weak MainWindowController* weakSelf = self;
        tab->client->SetFindResultCallback([weakSelf](int count, int activeMatch) {
            dispatch_async(dispatch_get_main_queue(), ^{
                MainWindowController* strongSelf = weakSelf;
                if (strongSelf && strongSelf.findBar) {
                    [strongSelf.findBar updateMatchCount:count activeMatch:activeMatch];
                }
            });
        });
    }

    if (_findBar.superview != self.window.contentView) {
        [self.window.contentView addSubview:_findBar];
    }
    _findBar.hidden = NO;
    [_findBar focusSearchField];
}

- (void)hideFindBar {
    if (_findBar) {
        // Stop finding and clear highlights
        Tab* tab = _tabManager->GetActiveTab();
        if (tab && tab->browser) {
            tab->browser->GetHost()->StopFinding(true);
        }

        // Clear the callback
        if (tab && tab->client) {
            tab->client->SetFindResultCallback(nullptr);
        }

        _findBar.hidden = YES;
        _lastSearchText = nil;

        // Return focus to browser
        if (tab && tab->browser) {
            tab->browser->GetHost()->SetFocus(true);
        }
    }
}

@end
