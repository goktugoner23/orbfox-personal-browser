#import "MainWindowController.h"
#import "SidebarView.h"
#import "ToolbarView.h"
#import "FindBarView.h"
#import "Components.h"
#import <QuartzCore/QuartzCore.h>

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

// ============================================================================
// DEVTOOLS CLIENT
// Minimal CefClient for DevTools that injects CSS to make room for close button
// ============================================================================

#include "include/cef_client.h"

// Forward declaration
@class MainWindowController;

class DevToolsClient : public CefClient,
                       public CefLifeSpanHandler,
                       public CefLoadHandler {
public:
    DevToolsClient() = default;

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        browser_ = nullptr;
    }

    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override {
        (void)httpStatusCode;
        if (!frame->IsMain()) return;

        // No JavaScript injection - we'll use a native header bar instead
        (void)browser;
        (void)frame;
    }

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

private:
    CefRefPtr<CefBrowser> browser_;

    IMPLEMENT_REFCOUNTING(DevToolsClient);
    DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

// ============================================================================
// DEVTOOLS DIVIDER VIEW
// Draggable divider between browser content and DevTools panel
// ============================================================================

static const CGFloat kDevToolsDividerWidth = 5.0;
static const CGFloat kDevToolsDefaultWidth = 420.0;
static const CGFloat kDevToolsMinWidth = 280.0;
static const CGFloat kDevToolsMaxWidth = 800.0;

@interface DevToolsDividerView : NSView
@property (nonatomic, weak) MainWindowController* windowController;
@end

@implementation DevToolsDividerView {
    NSTrackingArea* _trackingArea;
    BOOL _isDragging;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) [self removeTrackingArea:_trackingArea];
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
                                                  owner:self
                                               userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Subtle divider line
    [[NSColor colorWithWhite:0.0 alpha:0.3] setFill];
    NSRectFill(self.bounds);

    // Grip indicator: three small dots vertically centered
    NSColor* gripColor = [NSColor colorWithWhite:1.0 alpha:0.15];
    [gripColor setFill];
    CGFloat cx = NSMidX(self.bounds);
    CGFloat cy = NSMidY(self.bounds);
    CGFloat dotSize = 2.0;
    CGFloat dotSpacing = 5.0;
    for (int i = -1; i <= 1; i++) {
        NSRect dot = NSMakeRect(cx - dotSize / 2, cy + i * dotSpacing - dotSize / 2, dotSize, dotSize);
        [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
    }
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    [[NSCursor resizeLeftRightCursor] push];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (!_isDragging) [NSCursor pop];
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    _isDragging = YES;
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _isDragging = NO;
    [NSCursor pop];
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
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
// DevTools - child window approach (borderless window that tracks main window)
@property (nonatomic, readwrite) BOOL devToolsOpen;
@property (nonatomic, strong) NSWindow* devToolsWindow;         // The CEF-created DevTools window (styled borderless)
@property (nonatomic, strong) NSButton* devToolsCloseButton;    // Close button overlay
@property (nonatomic, strong) DevToolsDividerView* devToolsDivider;
@property (nonatomic, assign) CGFloat devToolsWidth;
@property (nonatomic, assign) BOOL devToolsAnimating;
@end

// Forward declare resizeDevToolsToWidth: for DevToolsDividerView
@interface MainWindowController ()
- (void)resizeDevToolsToWidth:(CGFloat)newWidth;
@end

// Implement DevToolsDividerView mouseDragged after MainWindowController is defined
@implementation DevToolsDividerView (Dragging)

- (void)mouseDragged:(NSEvent*)event {
    if (!_windowController) return;

    NSView* contentView = _windowController.window.contentView;
    NSPoint loc = [contentView convertPoint:event.locationInWindow fromView:nil];

    // DevTools width = distance from right edge of content view to mouse
    CGFloat rightEdge = contentView.bounds.size.width;
    CGFloat newWidth = rightEdge - loc.x;
    newWidth = MAX(kDevToolsMinWidth, MIN(kDevToolsMaxWidth, newWidth));

    [_windowController resizeDevToolsToWidth:newWidth];
}

@end

@implementation MainWindowController {
    CefRefPtr<DevToolsClient> _devToolsClient;
}

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

    // Handle open link in new tab from context menu
    client->SetOpenLinkCallback([weakSelf](const std::string& url, bool background) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf openLinkInNewTab:[NSString stringWithUTF8String:url.c_str()] background:background];
            });
        }
    });

    // Handle copy to clipboard from context menu
    client->SetCopyToClipboardCallback([](const std::string& text) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            [pasteboard setString:[NSString stringWithUTF8String:text.c_str()]
                          forType:NSPasteboardTypeString];
        });
    });

    client->SetInspectElementCallback([weakSelf](int x, int y) {
        dispatch_async(dispatch_get_main_queue(), ^{
            MainWindowController* strongSelf = weakSelf;
            if (strongSelf) {
                [strongSelf showDevToolsAtPoint:x y:y];
            }
        });
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

- (void)reopenClosedTab {
    if (_tabManager->ReopenClosedTab()) {
        // Tab was reopened - the browser will be created via the on_tab_created callback
        // which is already set up in setupTabManagerCallbacks
        [_sidebarView reloadWorkspaceTabs];
    }
}

- (void)openLinkInNewTab:(NSString*)url background:(BOOL)background {
    // Remember current active tab if opening in background
    Tab* previousActiveTab = background ? _tabManager->GetActiveTab() : nullptr;
    int previousTabId = previousActiveTab ? previousActiveTab->id : -1;

    // Create the new tab
    [self createNewTab:url];

    // If background, switch back to the previous tab
    if (background && previousTabId >= 0) {
        _tabManager->SetActiveTab(previousTabId);
    }
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

- (void)toggleDevTools {
    if (_devToolsOpen) {
        [self closeDevTools];
    } else {
        [self showDevToolsAtPoint:-1 y:-1];
    }
}

- (void)showDevToolsAtPoint:(int)x y:(int)y {
    Tab* tab = _tabManager->GetActiveTab();
    if (!tab || !tab->browser) return;

    // If already open, close first
    if (_devToolsOpen) {
        [self closeDevTools];
        // Small delay to let the previous DevTools close
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self openDevToolsForTab:tab atPoint:x y:y];
        });
        return;
    }

    [self openDevToolsForTab:tab atPoint:x y:y];
}

- (void)openDevToolsForTab:(Tab*)tab atPoint:(int)x y:(int)y {
    if (!tab || !tab->browser || _devToolsAnimating) return;

    _devToolsOpen = YES;
    _devToolsAnimating = YES;
    _devToolsWidth = kDevToolsDefaultWidth;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    CGFloat sidebarWidth = _sidebarView.frame.size.width;

    // Create divider if needed
    if (!_devToolsDivider) {
        _devToolsDivider = [[DevToolsDividerView alloc] init];
        _devToolsDivider.windowController = self;
        _devToolsDivider.autoresizingMask = NSViewHeightSizable;
        [contentView addSubview:_devToolsDivider];
    }

    // Divider starts hidden, will appear with animation
    _devToolsDivider.hidden = YES;

    // Create DevTools client that injects CSS to make room for close button
    _devToolsClient = new DevToolsClient();

    // Call ShowDevTools with our client
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;
    CefPoint inspectPoint;
    if (x >= 0 && y >= 0) {
        inspectPoint.Set(x, y);
    }

    tab->browser->GetHost()->ShowDevTools(windowInfo, _devToolsClient, settings, inspectPoint);

    // Immediately start looking for the DevTools window to hide it before it flashes
    [self findAndHideDevToolsWindow];
}

// DevTools toolbar height in pixels
static const CGFloat kDevToolsToolbarHeight = 28.0;

- (void)findAndHideDevToolsWindow {
    // Immediately look for the DevTools window to hide it before it flashes on screen
    NSWindow* devToolsWindow = nil;

    for (NSWindow* window in [NSApp windows]) {
        if (window != self.window && window != _devToolsWindow) {
            NSString* title = window.title;
            if ([title containsString:@"DevTools"] || [title containsString:@"Developer Tools"]) {
                devToolsWindow = window;
                break;
            }
        }
    }

    if (!devToolsWindow) {
        // Also check for windows without DevTools in title (CEF may not set it immediately)
        for (NSWindow* window in [NSApp windows]) {
            if (window != self.window && window != _devToolsWindow) {
                NSView* cv = window.contentView;
                if (cv && cv.subviews.count > 0) {
                    // Check if this looks like a CEF browser window
                    NSString* className = NSStringFromClass([cv.subviews.firstObject class]);
                    if ([className containsString:@"BrowserOpenGL"] || [className containsString:@"Cef"]) {
                        devToolsWindow = window;
                        break;
                    }
                }
            }
        }
    }

    if (devToolsWindow) {
        // Found it! Hide immediately by moving off-screen
        NSRect screenFrame = self.window.screen.frame;
        [devToolsWindow setFrame:NSMakeRect(screenFrame.size.width + 1000, 0, 400, 400) display:NO];

        // Now style and animate
        dispatch_async(dispatch_get_main_queue(), ^{
            [self styleDevToolsWindowAndAnimate:devToolsWindow];
        });
    } else {
        // Not found yet, retry on next run loop
        static int findRetryCount = 0;
        findRetryCount++;
        if (findRetryCount < 30) {  // Try for up to ~0.5 seconds
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.016 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [self findAndHideDevToolsWindow];
            });
        } else {
            findRetryCount = 0;
            _devToolsAnimating = NO;
            _devToolsOpen = NO;
            NSLog(@"Failed to find DevTools window");
        }
        return;
    }

    // Reset retry count on success
    static int findRetryCount = 0;
    findRetryCount = 0;
}

- (void)styleDevToolsWindowAndAnimate:(NSWindow*)devToolsWindow {
    if (!devToolsWindow) {
        _devToolsAnimating = NO;
        _devToolsOpen = NO;
        return;
    }

    // Store reference
    _devToolsWindow = devToolsWindow;

    // Style the window to be borderless
    _devToolsWindow.titlebarAppearsTransparent = YES;
    _devToolsWindow.titleVisibility = NSWindowTitleHidden;
    _devToolsWindow.styleMask = NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable;
    _devToolsWindow.backgroundColor = [NSColor colorWithRed:0.141 green:0.141 blue:0.157 alpha:1.0];  // #242428
    _devToolsWindow.hasShadow = NO;
    _devToolsWindow.movable = NO;
    _devToolsWindow.level = NSNormalWindowLevel;

    // Make it a child window
    [self.window addChildWindow:_devToolsWindow ordered:NSWindowAbove];

    // Find the CEF browser view inside the DevTools window and resize it to make room for header
    static const CGFloat kHeaderHeight = 28.0;
    NSView* devToolsContentView = _devToolsWindow.contentView;
    CGFloat windowHeight = devToolsContentView.bounds.size.height;

    // Create a header bar at the top with close button
    NSView* headerBar = [[NSView alloc] initWithFrame:NSMakeRect(0, windowHeight - kHeaderHeight, _devToolsWidth, kHeaderHeight)];
    headerBar.wantsLayer = YES;
    headerBar.layer.backgroundColor = [NSColor colorWithRed:0.141 green:0.141 blue:0.157 alpha:1.0].CGColor;  // Match DevTools bg
    headerBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;

    // Add a subtle bottom border to header
    NSView* headerBorder = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, _devToolsWidth, 1)];
    headerBorder.wantsLayer = YES;
    headerBorder.layer.backgroundColor = [NSColor colorWithWhite:0.0 alpha:0.3].CGColor;
    headerBorder.autoresizingMask = NSViewWidthSizable;
    [headerBar addSubview:headerBorder];

    // Create close button
    if (!_devToolsCloseButton) {
        _devToolsCloseButton = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 28, 28)];
        _devToolsCloseButton.bordered = NO;
        _devToolsCloseButton.wantsLayer = YES;
        _devToolsCloseButton.layer.cornerRadius = 4;
        _devToolsCloseButton.layer.backgroundColor = [NSColor clearColor].CGColor;

        NSImage* closeIcon = [NSImage imageWithSystemSymbolName:@"xmark"
                                       accessibilityDescription:@"Close DevTools"];
        NSImageSymbolConfiguration* config = [NSImageSymbolConfiguration configurationWithPointSize:10
                                                                                            weight:NSFontWeightMedium];
        closeIcon = [closeIcon imageWithSymbolConfiguration:config];
        _devToolsCloseButton.image = closeIcon;
        _devToolsCloseButton.contentTintColor = [NSColor colorWithRed:0.6 green:0.6 blue:0.63 alpha:1.0];
        _devToolsCloseButton.target = self;
        _devToolsCloseButton.action = @selector(closeDevTools);
        _devToolsCloseButton.toolTip = @"Close DevTools";

        NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
            initWithRect:_devToolsCloseButton.bounds
                 options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
                   owner:self
                userInfo:@{@"button": @"devToolsClose"}];
        [_devToolsCloseButton addTrackingArea:trackingArea];
    }

    // Position close button on right side of header
    CGFloat buttonSize = 24.0;
    _devToolsCloseButton.frame = NSMakeRect(_devToolsWidth - buttonSize - 22, (kHeaderHeight - buttonSize) / 2, buttonSize, buttonSize);
    _devToolsCloseButton.autoresizingMask = NSViewMinXMargin;
    [_devToolsCloseButton removeFromSuperview];
    [headerBar addSubview:_devToolsCloseButton];

    // Add "DevTools" label on left side
    NSTextField* titleLabel = [NSTextField labelWithString:@"DevTools"];
    titleLabel.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
    titleLabel.textColor = [NSColor colorWithRed:0.6 green:0.6 blue:0.63 alpha:1.0];
    titleLabel.frame = NSMakeRect(10, (kHeaderHeight - 16) / 2, 80, 16);
    [headerBar addSubview:titleLabel];

    // Add the header bar to the DevTools window
    [devToolsContentView addSubview:headerBar positioned:NSWindowAbove relativeTo:nil];

    // Resize the CEF browser view to be below the header
    for (NSView* subview in devToolsContentView.subviews) {
        if (subview != headerBar && subview != _devToolsCloseButton) {
            NSRect frame = subview.frame;
            frame.size.height = windowHeight - kHeaderHeight;
            frame.origin.y = 0;
            subview.frame = frame;
            subview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        }
    }

    // Calculate positions
    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    CGFloat sidebarWidth = _sidebarView.frame.size.width;
    CGFloat availableWidth = contentView.bounds.size.width - sidebarWidth;
    CGFloat browserWidth = availableWidth - _devToolsWidth - kDevToolsDividerWidth;

    // Calculate screen position for DevTools window
    NSRect mainWindowFrame = self.window.frame;
    NSRect contentRect = [self.window contentRectForFrameRect:mainWindowFrame];

    CGFloat startX = contentRect.origin.x + contentView.bounds.size.width + 10;  // Off-screen right
    CGFloat targetX = contentRect.origin.x + sidebarWidth + browserWidth + kDevToolsDividerWidth;
    CGFloat windowY = contentRect.origin.y + kToolbarHeight;

    // Set initial position (off-screen)
    [_devToolsWindow setFrame:NSMakeRect(startX, windowY, _devToolsWidth, browserHeight) display:YES];

    // Show divider
    CGFloat dividerX = sidebarWidth + browserWidth;
    _devToolsDivider.frame = NSMakeRect(contentView.bounds.size.width, kToolbarHeight, kDevToolsDividerWidth, browserHeight);
    _devToolsDivider.hidden = NO;
    _devToolsDivider.alphaValue = 0;

    // Animate slide-in
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = 0.25;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
        context.allowsImplicitAnimation = YES;

        // Slide DevTools window in
        [self->_devToolsWindow.animator setFrame:NSMakeRect(targetX, windowY, self->_devToolsWidth, browserHeight) display:YES];

        // Slide divider
        self->_devToolsDivider.animator.frame = NSMakeRect(dividerX, kToolbarHeight, kDevToolsDividerWidth, browserHeight);
        self->_devToolsDivider.animator.alphaValue = 1.0;

        // Shrink browser container
        self->_browserContainer.animator.frame = NSMakeRect(sidebarWidth, kToolbarHeight, browserWidth, browserHeight);

    } completionHandler:^{
        // Resize browser views
        for (NSView* subview in self->_browserContainer.subviews) {
            if (!subview.hidden) {
                subview.frame = self->_browserContainer.bounds;
            }
        }

        self->_devToolsAnimating = NO;
    }];
}

// Handle hover effects for close button
- (void)mouseEntered:(NSEvent*)event {
    NSDictionary* userData = event.trackingArea.userInfo;
    if ([userData[@"button"] isEqualToString:@"devToolsClose"]) {
        _devToolsCloseButton.layer.backgroundColor = [NSColor colorWithWhite:1.0 alpha:0.1].CGColor;
        _devToolsCloseButton.contentTintColor = [NSColor colorWithWhite:0.9 alpha:1.0];
    }
}

- (void)mouseExited:(NSEvent*)event {
    NSDictionary* userData = event.trackingArea.userInfo;
    if ([userData[@"button"] isEqualToString:@"devToolsClose"]) {
        _devToolsCloseButton.layer.backgroundColor = [NSColor clearColor].CGColor;
        _devToolsCloseButton.contentTintColor = [NSColor colorWithRed:0.6 green:0.6 blue:0.63 alpha:1.0];
    }
}

- (void)updateDevToolsWindowPosition {
    if (!_devToolsWindow || !_devToolsOpen) return;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    CGFloat sidebarWidth = _sidebarView.frame.size.width;
    CGFloat availableWidth = contentView.bounds.size.width - sidebarWidth;
    CGFloat browserWidth = availableWidth - _devToolsWidth - kDevToolsDividerWidth;

    // Calculate screen position
    NSRect mainWindowFrame = self.window.frame;
    NSRect contentRect = [self.window contentRectForFrameRect:mainWindowFrame];
    CGFloat windowX = contentRect.origin.x + sidebarWidth + browserWidth + kDevToolsDividerWidth;
    CGFloat windowY = contentRect.origin.y + kToolbarHeight;

    [_devToolsWindow setFrame:NSMakeRect(windowX, windowY, _devToolsWidth, browserHeight) display:YES];
    // Close button position is managed by its superview (header bar) with autoresizing
}

- (void)closeDevTools {
    if (!_devToolsOpen || _devToolsAnimating) return;

    // Close DevTools via CEF
    Tab* tab = _tabManager->GetActiveTab();
    if (tab && tab->browser) {
        tab->browser->GetHost()->CloseDevTools();
    }

    _devToolsOpen = NO;
    _devToolsAnimating = YES;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    CGFloat sidebarWidth = _sidebarView.frame.size.width;
    CGFloat fullWidth = contentView.bounds.size.width - sidebarWidth;

    // Calculate off-screen position
    NSRect mainWindowFrame = self.window.frame;
    NSRect contentRect = [self.window contentRectForFrameRect:mainWindowFrame];
    CGFloat offScreenX = contentRect.origin.x + contentView.bounds.size.width + 10;
    CGFloat windowY = contentRect.origin.y + kToolbarHeight;

    // Animate slide-out
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = 0.22;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
        context.allowsImplicitAnimation = YES;

        // Slide DevTools window out
        if (self->_devToolsWindow) {
            [self->_devToolsWindow.animator setFrame:NSMakeRect(offScreenX, windowY, self->_devToolsWidth, browserHeight) display:YES];
        }

        // Slide divider out
        self->_devToolsDivider.animator.frame = NSMakeRect(contentView.bounds.size.width, kToolbarHeight,
                                                            kDevToolsDividerWidth, browserHeight);
        self->_devToolsDivider.animator.alphaValue = 0;

        // Expand browser container
        self->_browserContainer.animator.frame = NSMakeRect(sidebarWidth, kToolbarHeight, fullWidth, browserHeight);

    } completionHandler:^{
        // Hide divider
        self->_devToolsDivider.hidden = YES;

        // Close DevTools window
        if (self->_devToolsWindow) {
            [self.window removeChildWindow:self->_devToolsWindow];
            [self->_devToolsWindow close];
            self->_devToolsWindow = nil;
        }

        // Remove close button
        if (self->_devToolsCloseButton) {
            [self->_devToolsCloseButton removeFromSuperview];
            self->_devToolsCloseButton = nil;
        }

        // Release DevTools client
        self->_devToolsClient = nullptr;

        // Resize browser views
        for (NSView* subview in self->_browserContainer.subviews) {
            if (!subview.hidden) {
                subview.frame = self->_browserContainer.bounds;
            }
        }

        self->_devToolsAnimating = NO;
    }];
}

- (void)resizeDevToolsToWidth:(CGFloat)newWidth {
    if (!_devToolsOpen || _devToolsAnimating) return;

    _devToolsWidth = newWidth;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;
    CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
    CGFloat sidebarWidth = _sidebarView.frame.size.width;
    CGFloat browserWidth = contentView.bounds.size.width - sidebarWidth - newWidth - kDevToolsDividerWidth;

    _browserContainer.frame = NSMakeRect(sidebarWidth, kToolbarHeight, browserWidth, browserHeight);
    _devToolsDivider.frame = NSMakeRect(sidebarWidth + browserWidth, kToolbarHeight, kDevToolsDividerWidth, browserHeight);

    // Resize browser views
    for (NSView* subview in _browserContainer.subviews) {
        if (!subview.hidden) {
            subview.frame = _browserContainer.bounds;
        }
    }

    // Update DevTools window position and size
    [self updateDevToolsWindowPosition];
}

- (void)switchToPreviousWorkspace {
    const auto& workspaces = _tabManager->GetWorkspaces();
    if (workspaces.size() <= 1) return;

    Workspace* active = _tabManager->GetActiveWorkspace();
    if (!active) return;

    for (size_t i = 0; i < workspaces.size(); ++i) {
        if (workspaces[i]->id == active->id) {
            int prevIndex = (i == 0) ? (int)workspaces.size() - 1 : (int)i - 1;
            _tabManager->SetActiveWorkspace(workspaces[prevIndex]->id);
            [_sidebarView reloadWorkspaceTabs];
            [_sidebarView reloadTabs];

            Tab* activeTab = _tabManager->GetActiveTab();
            if (activeTab) {
                [self activateTab:activeTab->id];
            }
            return;
        }
    }
}

- (void)switchToNextWorkspace {
    const auto& workspaces = _tabManager->GetWorkspaces();
    if (workspaces.size() <= 1) return;

    Workspace* active = _tabManager->GetActiveWorkspace();
    if (!active) return;

    for (size_t i = 0; i < workspaces.size(); ++i) {
        if (workspaces[i]->id == active->id) {
            int nextIndex = (i + 1 >= workspaces.size()) ? 0 : (int)i + 1;
            _tabManager->SetActiveWorkspace(workspaces[nextIndex]->id);
            [_sidebarView reloadWorkspaceTabs];
            [_sidebarView reloadTabs];

            Tab* activeTab = _tabManager->GetActiveTab();
            if (activeTab) {
                [self activateTab:activeTab->id];
            }
            return;
        }
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

    if (_devToolsOpen && !_devToolsAnimating) {
        // Account for DevTools panel
        CGFloat availableWidth = contentView.bounds.size.width - newWidth;
        CGFloat browserWidth = availableWidth - _devToolsWidth - kDevToolsDividerWidth;
        _browserContainer.frame = NSMakeRect(newWidth, kToolbarHeight, browserWidth, browserHeight);
        _devToolsDivider.frame = NSMakeRect(newWidth + browserWidth, kToolbarHeight, kDevToolsDividerWidth, browserHeight);

        // Update DevTools window position
        [self updateDevToolsWindowPosition];
    } else {
        _browserContainer.frame = NSMakeRect(newWidth, kToolbarHeight,
                                              contentView.bounds.size.width - newWidth, browserHeight);
    }

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
        CGFloat availableWidth = contentView.bounds.size.width - newSidebarWidth;
        CGFloat browserWidth = availableWidth;

        if (self->_devToolsOpen && !self->_devToolsAnimating) {
            browserWidth = availableWidth - self->_devToolsWidth - kDevToolsDividerWidth;
            self->_devToolsDivider.animator.frame = NSMakeRect(newSidebarWidth + browserWidth, kToolbarHeight,
                                                                kDevToolsDividerWidth, browserHeight);
        }

        NSRect browserFrame = NSMakeRect(newSidebarWidth, kToolbarHeight, browserWidth, browserHeight);
        _browserContainer.animator.frame = browserFrame;

    } completionHandler:^{
        // Resize browser views to fit new container
        for (NSView* subview in self->_browserContainer.subviews) {
            if (!subview.hidden) {
                subview.frame = self->_browserContainer.bounds;
            }
        }

        // Update DevTools window position if open
        if (self->_devToolsOpen && !self->_devToolsAnimating) {
            [self updateDevToolsWindowPosition];
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
            BOOL hasShift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
            BOOL hasOption = (event.modifierFlags & NSEventModifierFlagOption) != 0;

            // Cmd+Opt shortcuts
            if (hasOption) {
                if ([chars isEqualToString:@"i"]) {
                    // Cmd+Opt+I: Toggle DevTools
                    [self toggleDevTools];
                    return nil;
                }

                // Cmd+Opt+Left/Right: Switch workspace
                if (event.keyCode == 123) {  // Left arrow
                    [self switchToPreviousWorkspace];
                    return nil;
                } else if (event.keyCode == 124) {  // Right arrow
                    [self switchToNextWorkspace];
                    return nil;
                }
            }

            if ([chars isEqualToString:@"t"] && hasShift) {
                // Cmd+Shift+T: Reopen closed tab
                [self reopenClosedTab];
                return nil;
            } else if ([chars isEqualToString:@"t"] && !hasShift) {
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
    // Close DevTools if open (CEF will close its window)
    [self closeDevTools];
    CefQuitMessageLoop();
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (_devToolsOpen && !_devToolsAnimating) {
        NSView* contentView = self.window.contentView;
        CGFloat titleBarHeight = 28;
        CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
        CGFloat sidebarWidth = _sidebarView.frame.size.width;
        CGFloat availableWidth = contentView.bounds.size.width - sidebarWidth;
        CGFloat browserWidth = availableWidth - _devToolsWidth - kDevToolsDividerWidth;

        _devToolsDivider.frame = NSMakeRect(sidebarWidth + browserWidth, kToolbarHeight, kDevToolsDividerWidth, browserHeight);

        // Update DevTools window position
        [self updateDevToolsWindowPosition];
    }
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
