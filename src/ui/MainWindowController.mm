#import "MainWindowController.h"
#import "SidebarView.h"
#import "ToolbarView.h"
#import "FindBarView.h"
#import "AccountPopover.h"
#import "Components.h"
#import "GestureContainerView.h"
#import <QuartzCore/QuartzCore.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "history_storage.h"
#include "bookmark_storage.h"
#include "download_manager.h"
#include "settings_storage.h"
#include "session_storage.h"
#include "DevToolsClient.h"

// Defined in browser_app.mm
extern void SaveSession();

// Layout constants
static const CGFloat kTitleBarHeight = 28.0;
static const CGFloat kSidebarDefaultWidth = 280.0;
static const CGFloat kSidebarCollapsedWidth = 44.0;
static const CGFloat kToolbarHeight = 44.0;
static const CGFloat kResizeHandleWidth = 6.0;
static const CGFloat kDevToolsDividerWidth = 5.0;
static const CGFloat kDevToolsDefaultWidth = 420.0;
static const CGFloat kDevToolsMinWidth = 280.0;
static const CGFloat kDevToolsMaxWidth = 800.0;


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
// Account popover
@property (nonatomic, strong) AccountPopoverController* accountPopover;
@end

// Forward declare resizeDevToolsToWidth: for DevToolsDividerView
@interface MainWindowController ()
- (void)resizeDevToolsToWidth:(CGFloat)newWidth;
@end

// Implement DevToolsDividerView mouseDragged after MainWindowController is defined
@implementation DevToolsDividerView (Dragging)

- (void)mouseDragged:(NSEvent*)event {
    if (!self.windowController) return;

    NSView* contentView = self.windowController.window.contentView;
    NSPoint loc = [contentView convertPoint:event.locationInWindow fromView:nil];

    // DevTools width = distance from right edge of content view to mouse
    CGFloat rightEdge = contentView.bounds.size.width;
    CGFloat newWidth = rightEdge - loc.x;
    newWidth = MAX(kDevToolsMinWidth, MIN(kDevToolsMaxWidth, newWidth));

    [self.windowController resizeDevToolsToWidth:newWidth];
}

@end

// Loading indicator timing constants (industry standard values)
static const NSTimeInterval kLoadingIndicatorDelay = 0.4;      // 400ms delay before showing
static const NSTimeInterval kLoadingIndicatorMinDuration = 0.2; // 200ms minimum display time

@implementation MainWindowController {
    CefRefPtr<DevToolsClient> _devToolsClient;
    id _eventMonitor;
    id _flagsChangedMonitor;

    // Loading indicator debouncing state (per-tab)
    NSMutableDictionary<NSNumber*, NSNumber*>* _tabRealNavigationFlags;  // tabId -> BOOL (is real navigation)
    NSMutableDictionary<NSNumber*, NSDate*>* _tabLoadingStartTimes;      // tabId -> start time
    NSMutableDictionary<NSNumber*, NSNumber*>* _tabLoadingGeneration;    // tabId -> generation counter (for cancellation)

    // Content fullscreen state (HTML5 Fullscreen API)
    BOOL _isContentFullscreen;
    NSRect _savedBrowserContainerFrame;
    NSRect _savedSidebarFrame;
    NSRect _savedToolbarFrame;

    // Tab hibernation timer
    NSTimer* _hibernationTimer;
    NSTimeInterval _lastBrowserActivityTime;  // Last time user interacted with any tab

    // Shutdown state
    BOOL _isShuttingDown;

    // Focus URL bar after async browser creation completes
    BOOL _pendingURLBarFocus;
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

        // Initialize loading indicator debouncing state
        _tabRealNavigationFlags = [NSMutableDictionary new];
        _tabLoadingStartTimes = [NSMutableDictionary new];
        _tabLoadingGeneration = [NSMutableDictionary new];
        _lastBrowserActivityTime = [NSDate timeIntervalSinceReferenceDate];

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
        [self startHibernationTimer];

        [window center];
    }
    return self;
}

- (void)startHibernationTimer {
    // Check for inactive tabs every 60 seconds
    _hibernationTimer = [NSTimer scheduledTimerWithTimeInterval:60.0
                                                         target:self
                                                       selector:@selector(checkInactiveTabs)
                                                       userInfo:nil
                                                        repeats:YES];
}

- (void)checkInactiveTabs {
    if (!_tabManager) return;

    // Only hibernate when the entire browser has been idle for 5 minutes.
    // If the user is actively browsing (any tab), don't hibernate other tabs.
    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    if (now - _lastBrowserActivityTime < 300.0) return;

    _tabManager->HibernateInactiveTabs(300);
}

- (void)setupViews {
    NSView* contentView = self.window.contentView;
    NSRect bounds = contentView.bounds;

    // Account for title bar
    CGFloat titleBarHeight = kTitleBarHeight;
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

    // Browser container (with gesture support)
    CGFloat browserX = _currentSidebarWidth;
    CGFloat browserY = kToolbarHeight;
    CGFloat browserWidth = bounds.size.width - _currentSidebarWidth;
    CGFloat browserHeight = contentHeight - kToolbarHeight;

    GestureContainerView* gestureContainer = [[GestureContainerView alloc] initWithFrame:NSMakeRect(browserX, browserY, browserWidth, browserHeight)];
    gestureContainer.wantsLayer = YES;
    gestureContainer.layer.backgroundColor = [NSColor blackColor].CGColor;
    gestureContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    gestureContainer.gestureDelegate = self;
    _browserContainer = gestureContainer;
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

        // Remove browser view synchronously — we're already on the main thread.
        // Deferring via dispatch_async creates a race with on_tab_hibernated blocks
        // that can close the browser and free the NSView before this block runs.
        CefRefPtr<CefBrowser> browser = tab->browser;
        [strongSelf removeBrowserView:browser];

        // Defer sidebar reload until after CloseTab finishes erasing the tab
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.sidebarView reloadTabs];
        });
    };

    callbacks.on_tab_activated = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // Update active time for hibernation tracking
        strongSelf->_lastBrowserActivityTime = [NSDate timeIntervalSinceReferenceDate];
        strongSelf.tabManager->UpdateTabActiveTime(tab->id);

        // If tab is hibernated, wake it (this will trigger on_tab_woken callback)
        if (tab->is_hibernated) {
            strongSelf.tabManager->WakeTab(tab->id);
            return;  // on_tab_woken will handle showing the browser
        }

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

            // Set gear icon for internal orbfox:// pages
            NSString* urlStr = [NSString stringWithUTF8String:url.c_str()];
            if ([urlStr hasPrefix:@"orbfox://"]) {
                [strongSelf.sidebarView updateTabWithGearIcon:tabId];
            }
            // Update favicon if we have data
            else if (!faviconData.empty()) {
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

    // Tab hibernation: close browser to free memory
    callbacks.on_tab_hibernated = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // We detach the view and null out tab pointers synchronously to avoid UAF
        // (see previous comment about raw tab pointer in async blocks).
        // But we delay CloseBrowser so the page can save state first.
        CefRefPtr<CefBrowser> browser = tab->browser;
        if (browser) {
            CefRefPtr<CefBrowserHost> host = browser->GetHost();
            if (host) {
                // Notify the page it's being hidden so it can save state
                // (e.g. YouTube saves video playback position on visibilitychange)
                CefRefPtr<CefFrame> frame = browser->GetMainFrame();
                if (frame) {
                    frame->ExecuteJavaScript(
                        "try {"
                        "  Object.defineProperty(document, 'visibilityState', "
                        "    {value: 'hidden', configurable: true});"
                        "  Object.defineProperty(document, 'hidden', "
                        "    {value: true, configurable: true});"
                        "  document.dispatchEvent(new Event('visibilitychange'));"
                        "  window.dispatchEvent(new PageTransitionEvent('pagehide', "
                        "    {persisted: true}));"
                        "} catch(e) {}",
                        frame->GetURL(), 0);
                }

                void* windowHandle = host->GetWindowHandle();
                if (windowHandle) {
                    for (NSView* subview in [strongSelf->_browserContainer.subviews copy]) {
                        if ((__bridge void*)subview == windowHandle) {
                            [subview removeFromSuperview];
                            break;
                        }
                    }
                }

                // Delay browser close to let the page complete save operations
                dispatch_after(
                    dispatch_time(DISPATCH_TIME_NOW, (int64_t)(500 * NSEC_PER_MSEC)),
                    dispatch_get_main_queue(), ^{
                        host->CloseBrowser(true);
                    });
            }
        }
        tab->browser = nullptr;
        tab->client = nullptr;

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.sidebarView reloadTabs];
        });
    };

    // Tab woken: recreate browser
    callbacks.on_tab_woken = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // Capture data before async
        int tabId = tab->id;
        std::string url = tab->url;

        dispatch_async(dispatch_get_main_queue(), ^{
            // Recreate the browser for this tab
            Tab* t = strongSelf.tabManager->GetTabById(tabId);
            if (t) {
                [strongSelf createBrowserForTab:t];
                [strongSelf.sidebarView reloadTabs];
            }
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
        if (!tab) {
            // Tab was closed while async CreateBrowser was in flight.
            // Close the orphaned browser and remove its view.
            CefRefPtr<CefBrowserHost> host = browser->GetHost();
            if (host) {
                void* windowHandle = host->GetWindowHandle();
                if (windowHandle) {
                    for (NSView* subview in [strongSelf->_browserContainer.subviews copy]) {
                        if ((__bridge void*)subview == windowHandle) {
                            [subview removeFromSuperview];
                            break;
                        }
                    }
                }
                host->CloseBrowser(true);
            }
            return;
        }

        tab->browser = browser;

        dispatch_async(dispatch_get_main_queue(), ^{
            // Configure the browser view
            CefRefPtr<CefBrowserHost> host = browser->GetHost();
            if (host) {
                NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
                if (browserView) {
                    browserView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

                    // Show this browser only if it's the active tab, otherwise hide it
                    if (strongSelf.tabManager->GetActiveTab() == tab) {
                        [strongSelf showBrowserForTab:tab];
                        if (strongSelf->_pendingURLBarFocus) {
                            strongSelf->_pendingURLBarFocus = NO;
                            [strongSelf.toolbarView focusURLField];
                        }
                    } else {
                        // Hide the browser view for background tabs
                        browserView.hidden = YES;
                    }
                }
            }
        });
    });

    // When browser is closed externally (JS window.close(), renderer crash, etc.),
    // null out tab->browser so we never use a stale CefRefPtr whose native view is freed.
    client->SetCloseCallback([weakSelf, tabId]() {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        Tab* tab = strongSelf.tabManager->GetTabById(tabId);
        if (tab) {
            // Remove the view from container before it becomes dangling
            if (tab->browser) {
                CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
                if (host) {
                    void* windowHandle = host->GetWindowHandle();
                    if (windowHandle) {
                        for (NSView* subview in [strongSelf->_browserContainer.subviews copy]) {
                            if ((__bridge void*)subview == windowHandle) {
                                [subview removeFromSuperview];
                                break;
                            }
                        }
                    }
                }
            }
            tab->browser = nullptr;
            tab->client = nullptr;
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

            // Copy URL to avoid dangling reference - the original string is destroyed
            // before dispatch_async block executes
            std::string urlCopy = url;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (strongSelf.tabManager->GetActiveTab() &&
                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                    [strongSelf.toolbarView setURL:[NSString stringWithUTF8String:urlCopy.c_str()]];
                }
            });
        }
    });

    // Track real navigations (OnLoadStart) to distinguish from JavaScript-triggered loading states
    client->SetNavigationStartCallback([weakSelf, tabId]() {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf) return;

        dispatch_async(dispatch_get_main_queue(), ^{
            NSNumber* tabKey = @(tabId);
            strongSelf->_tabRealNavigationFlags[tabKey] = @YES;
        });
    });

    client->SetLoadingStateCallback([weakSelf, tabId](bool isLoading, bool canGoBack, bool canGoForward) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf && strongSelf.tabManager) {
            // Record history when page finishes loading (do this immediately, not debounced)
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
                NSNumber* tabKey = @(tabId);
                BOOL isRealNavigation = [strongSelf->_tabRealNavigationFlags[tabKey] boolValue];

                if (isLoading) {
                    // Increment generation to invalidate any pending delayed show blocks
                    NSInteger currentGen = [strongSelf->_tabLoadingGeneration[tabKey] integerValue] + 1;
                    strongSelf->_tabLoadingGeneration[tabKey] = @(currentGen);

                    // Only show loading indicator for real navigations, with delay
                    if (isRealNavigation) {
                        NSInteger capturedGen = currentGen;
                        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kLoadingIndicatorDelay * NSEC_PER_SEC)),
                                       dispatch_get_main_queue(), ^{
                            // Check if this block is still valid (generation hasn't changed)
                            NSInteger nowGen = [strongSelf->_tabLoadingGeneration[tabKey] integerValue];
                            if (capturedGen != nowGen) return;  // Cancelled

                            // Show loading indicator after delay
                            strongSelf.tabManager->UpdateTabLoadingState(tabId, true);
                            strongSelf->_tabLoadingStartTimes[tabKey] = [NSDate date];

                            if (strongSelf.tabManager->GetActiveTab() &&
                                strongSelf.tabManager->GetActiveTab()->id == tabId) {
                                [strongSelf.toolbarView setLoading:YES];
                            }
                        });
                    }
                } else {
                    // Loading finished - increment generation to cancel pending show block
                    NSInteger currentGen = [strongSelf->_tabLoadingGeneration[tabKey] integerValue] + 1;
                    strongSelf->_tabLoadingGeneration[tabKey] = @(currentGen);

                    // Check if we need to enforce minimum display time
                    NSDate* startTime = strongSelf->_tabLoadingStartTimes[tabKey];
                    if (startTime) {
                        NSTimeInterval elapsed = -[startTime timeIntervalSinceNow];
                        NSTimeInterval remaining = kLoadingIndicatorMinDuration - elapsed;

                        if (remaining > 0) {
                            // Keep spinner visible for minimum duration
                            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(remaining * NSEC_PER_SEC)),
                                           dispatch_get_main_queue(), ^{
                                strongSelf.tabManager->UpdateTabLoadingState(tabId, false);
                                if (strongSelf.tabManager->GetActiveTab() &&
                                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                                    [strongSelf.toolbarView setLoading:NO];
                                }
                            });
                        } else {
                            // Minimum duration already elapsed, hide immediately
                            strongSelf.tabManager->UpdateTabLoadingState(tabId, false);
                            if (strongSelf.tabManager->GetActiveTab() &&
                                strongSelf.tabManager->GetActiveTab()->id == tabId) {
                                [strongSelf.toolbarView setLoading:NO];
                            }
                        }
                        strongSelf->_tabLoadingStartTimes[tabKey] = nil;
                    } else {
                        // Loading indicator was never shown (fast load), just update state
                        strongSelf.tabManager->UpdateTabLoadingState(tabId, false);
                        if (strongSelf.tabManager->GetActiveTab() &&
                            strongSelf.tabManager->GetActiveTab()->id == tabId) {
                            [strongSelf.toolbarView setLoading:NO];
                        }
                    }

                    // Reset navigation flag
                    strongSelf->_tabRealNavigationFlags[tabKey] = @NO;
                }

                // Always update navigation buttons immediately
                if (strongSelf.tabManager->GetActiveTab() &&
                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                    [strongSelf.toolbarView setCanGoBack:canGoBack canGoForward:canGoForward];
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
            // Copy URL to avoid dangling reference
            std::string urlCopy = url;
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf createNewTab:[NSString stringWithUTF8String:urlCopy.c_str()]];
            });
        }
    });

    // Handle open link in new tab from context menu
    client->SetOpenLinkCallback([weakSelf](const std::string& url, bool background) {
        MainWindowController* strongSelf = weakSelf;
        if (strongSelf) {
            // Copy URL to avoid dangling reference - the original string is destroyed
            // before dispatch_async block executes
            std::string urlCopy = url;
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf openLinkInNewTab:[NSString stringWithUTF8String:urlCopy.c_str()] background:background];
            });
        }
    });

    // Handle copy to clipboard from context menu
    client->SetCopyToClipboardCallback([](const std::string& text) {
        // Copy text to avoid dangling reference
        std::string textCopy = text;
        dispatch_async(dispatch_get_main_queue(), ^{
            NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            [pasteboard setString:[NSString stringWithUTF8String:textCopy.c_str()]
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

    client->SetFocusUrlBarCallback([weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            MainWindowController* strongSelf = weakSelf;
            if (strongSelf && strongSelf->_toolbarView) {
                [strongSelf->_toolbarView focusURLField];
            }
        });
    });

    // Handle bookmark actions from orbfox://bookmarks page and context menu
    client->SetBookmarkActionCallback([weakSelf](const std::string& action, const std::string& param) {
        std::string actionCopy = action;
        std::string paramCopy = param;
        dispatch_async(dispatch_get_main_queue(), ^{
            MainWindowController* strongSelf = weakSelf;
            if (!strongSelf) return;

            if (actionCopy == "add") {
                // Show add bookmark dialog for current page (from context menu)
                Tab* activeTab = strongSelf->_tabManager->GetActiveTab();
                if (activeTab) {
                    NSString* url = [NSString stringWithUTF8String:activeTab->url.c_str()];
                    NSString* title = [NSString stringWithUTF8String:activeTab->title.c_str()];
                    [strongSelf.sidebarView showAddBookmarkSheetWithUrl:url title:title];
                }
            } else if (actionCopy == "add_link") {
                // Show add bookmark dialog for a specific link (from context menu)
                // param format: "url\ttitle"
                size_t tabPos = paramCopy.find('\t');
                std::string url = (tabPos != std::string::npos) ? paramCopy.substr(0, tabPos) : paramCopy;
                std::string title = (tabPos != std::string::npos) ? paramCopy.substr(tabPos + 1) : url;
                NSString* nsUrl = [NSString stringWithUTF8String:url.c_str()];
                NSString* nsTitle = [NSString stringWithUTF8String:title.c_str()];
                [strongSelf.sidebarView showAddBookmarkSheetWithUrl:nsUrl title:nsTitle];
            } else if (actionCopy == "edit") {
                // Edit bookmark - show edit dialog with bookmark ID
                int64_t bookmarkId = std::stoll(paramCopy);
                [strongSelf.sidebarView showEditBookmarkDialog:bookmarkId];
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

    // Provide screen rect for popup centering (uses the window's current screen)
    client->SetPopupRectCallback([weakSelf](int& x, int& y, int& w, int& h) {
        MainWindowController* strongSelf = weakSelf;
        NSScreen* screen = strongSelf ? strongSelf.window.screen : nil;
        if (!screen) screen = [NSScreen mainScreen];
        NSRect frame = screen.frame;
        x = (int)frame.origin.x;
        y = (int)frame.origin.y;
        w = (int)frame.size.width;
        h = (int)frame.size.height;
    });

    // Create browser settings
    CefBrowserSettings settings;

    // Initial URL - use settings for new tab URL
    std::string url = tab->url.empty() ? SettingsStorage::GetInstance().Get().new_tab_url : tab->url;

    // Window info - embed in our browser container
    CefWindowInfo window_info;
    NSRect bounds = _browserContainer.bounds;
    CefRect cef_rect(0, 0, bounds.size.width, bounds.size.height);
    window_info.SetAsChild((__bridge void*)_browserContainer, cef_rect);

    // Create browser asynchronously
    CefBrowserHost::CreateBrowser(window_info, client, url, settings, nullptr, nullptr);
}

- (void)removeBrowserView:(CefRefPtr<CefBrowser>)browser {
    if (!browser) return;

    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (host) {
        void* windowHandle = host->GetWindowHandle();
        if (windowHandle) {
            // Validate the view is still in our hierarchy before using it.
            // GetWindowHandle() can return a stale pointer to a freed NSView.
            for (NSView* subview in [_browserContainer.subviews copy]) {
                if ((__bridge void*)subview == windowHandle) {
                    [subview removeFromSuperview];
                    break;
                }
            }
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
    // Delegate to showBrowserWithRef: to avoid code duplication
    if (tab && tab->browser) {
        [self showBrowserWithRef:tab->browser];
    } else {
        // Just hide all views when no valid browser
        for (NSView* subview in _browserContainer.subviews) {
            subview.hidden = YES;
        }
    }
}

#pragma mark - Tab Operations

- (void)createNewTab:(NSString*)url {
    std::string urlStr = url ? [url UTF8String] : "";
    _pendingURLBarFocus = YES;
    _tabManager->CreateTab(urlStr);
}

- (void)createBackgroundTab:(NSString*)url {
    std::string urlStr = url ? [url UTF8String] : "";
    _tabManager->CreateTabInBackground(urlStr);
}

#pragma mark - Reusable Browser Actions

- (void)openUrlInCurrentTab:(NSString*)url {
    if (!url || url.length == 0) return;
    [self navigateToURL:url];
}

- (void)openUrlInNewTab:(NSString*)url {
    [self createNewTab:url];
}

- (void)openUrlInBackgroundTab:(NSString*)url {
    [self createBackgroundTab:url];
}

- (void)copyUrlToClipboard:(NSString*)url {
    if (!url || url.length == 0) return;
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:url forType:NSPasteboardTypeString];
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
    if (background) {
        [self openUrlInBackgroundTab:url];
    } else {
        [self openUrlInNewTab:url];
    }
}

- (void)openSettingsInNewTab {
    [self createNewTab:@"orbfox://settings"];
}

#pragma mark - Navigation

- (void)navigateToURL:(NSString*)url {
    Tab* tab = _tabManager->GetActiveTab();
    if (!tab || !tab->browser) return;

    std::string resolved = SettingsStorage::GetInstance().ResolveAddressBarInput([url UTF8String]);
    tab->browser->GetMainFrame()->LoadURL(resolved);
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

#pragma mark - GestureContainerDelegate

- (void)gestureRecognized:(GestureType)gesture {
    switch (gesture) {
        case GestureTypeLeft:
            // Drag left → Go back
            [self goBack];
            break;
        case GestureTypeRight:
            // Drag right → Go forward
            [self goForward];
            break;
        case GestureTypeLShape:
            // L-shape (down then right) → Close tab
            [self closeCurrentTab];
            break;
        case GestureTypeReverseLShape:
            // Reverse L-shape (down then left) → Reopen closed tab
            [self reopenClosedTab];
            break;
        case GestureTypeNone:
            break;
    }
}

- (void)setFullscreen:(BOOL)fullscreen {
    NSWindow* window = self.window;
    NSView* contentView = window.contentView;

    // Get the active tab's browser client to track fullscreen state
    Tab* activeTab = _tabManager->GetActiveTab();
    CefRefPtr<BrowserClient> browserClient = activeTab ? activeTab->client : nullptr;

    if (fullscreen && !_isContentFullscreen) {
        // Entering content fullscreen
        _isContentFullscreen = YES;

        // Track fullscreen state in browser client (for keyboard event filtering)
        if (browserClient) {
            browserClient->SetContentFullscreen(true);
        }

        // Save current frames
        _savedBrowserContainerFrame = _browserContainer.frame;
        _savedSidebarFrame = _sidebarView.frame;
        _savedToolbarFrame = _toolbarView.frame;

        // Close DevTools if open
        if (_devToolsOpen) {
            [self closeDevTools];
        }

        // Hide find bar if visible
        if (_findBar && !_findBar.hidden) {
            [self hideFindBar];
        }

        // Hide UI elements
        _sidebarView.hidden = YES;
        _toolbarView.hidden = YES;
        _resizeHandle.hidden = YES;

        // Disable autoresizing temporarily for manual layout
        _browserContainer.autoresizingMask = NSViewNotSizable;

        // Expand browser container to fill entire window
        _browserContainer.frame = contentView.bounds;

        // Also resize the actual browser view inside the container
        for (NSView* subview in _browserContainer.subviews) {
            if ([subview isKindOfClass:[NSView class]]) {
                subview.frame = _browserContainer.bounds;
            }
        }

        // Re-enable autoresizing to fill window
        _browserContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // Enter macOS fullscreen if not already
        BOOL isCurrentlyFullscreen = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
        if (!isCurrentlyFullscreen) {
            [window toggleFullScreen:nil];
        }

    } else if (!fullscreen && _isContentFullscreen) {
        // Exiting content fullscreen
        _isContentFullscreen = NO;

        // Track fullscreen state in browser client
        if (browserClient) {
            browserClient->SetContentFullscreen(false);
        }

        // Exit macOS fullscreen if currently in it
        BOOL isCurrentlyFullscreen = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
        if (isCurrentlyFullscreen) {
            [window toggleFullScreen:nil];
        }

        // Show UI elements
        _sidebarView.hidden = NO;
        _toolbarView.hidden = NO;
        _resizeHandle.hidden = NO;

        // Refresh sidebar panel visibility (ensures internal panels are shown)
        [_sidebarView showPanel:_sidebarView.activePanel];

        // Restore browser container to normal layout
        CGFloat titleBarHeight = kTitleBarHeight;
        CGFloat sidebarWidth = _currentSidebarWidth;
        CGFloat browserX = sidebarWidth;
        CGFloat browserY = kToolbarHeight;
        CGFloat browserWidth = contentView.bounds.size.width - sidebarWidth;
        CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;

        _browserContainer.autoresizingMask = NSViewNotSizable;
        _browserContainer.frame = NSMakeRect(browserX, browserY, browserWidth, browserHeight);
        _browserContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // Resize browser views to fit container
        for (NSView* subview in _browserContainer.subviews) {
            if ([subview isKindOfClass:[NSView class]]) {
                subview.frame = _browserContainer.bounds;
            }
        }
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
    CGFloat titleBarHeight = kTitleBarHeight;
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

    // Static retry counter for finding DevTools window
    static int findRetryCount = 0;

    if (devToolsWindow) {
        // Found it! Reset retry count and hide immediately
        findRetryCount = 0;
        NSRect screenFrame = self.window.screen.frame;
        [devToolsWindow setFrame:NSMakeRect(screenFrame.size.width + 1000, 0, 400, 400) display:NO];

        // Now style and animate
        dispatch_async(dispatch_get_main_queue(), ^{
            [self styleDevToolsWindowAndAnimate:devToolsWindow];
        });
    } else {
        // Not found yet, retry on next run loop
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
}

- (void)styleDevToolsWindowAndAnimate:(NSWindow*)devToolsWindow {
    if (!devToolsWindow) {
        _devToolsAnimating = NO;
        _devToolsOpen = NO;
        return;
    }

    // Store reference
    _devToolsWindow = devToolsWindow;

    // Style the window - keep titlebar but make it minimal/hidden
    // IMPORTANT: Don't change styleMask to borderless (crashes due to CEF KVO observers)
    // and don't use FullSizeContentView (invisible titlebar captures mouse events)
    _devToolsWindow.titlebarAppearsTransparent = YES;
    _devToolsWindow.titleVisibility = NSWindowTitleHidden;
    _devToolsWindow.backgroundColor = [DSColors devToolsBackground];
    _devToolsWindow.hasShadow = NO;
    _devToolsWindow.movable = NO;

    // Hide the standard window buttons (close, minimize, zoom)
    [[_devToolsWindow standardWindowButton:NSWindowCloseButton] setHidden:YES];
    [[_devToolsWindow standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[_devToolsWindow standardWindowButton:NSWindowZoomButton] setHidden:YES];

    // Make it a child window so it follows the main window
    [self.window addChildWindow:_devToolsWindow ordered:NSWindowAbove];

    // Add close button in the titlebar area — CEF's compositor paints over regular NSViews,
    // but NSTitlebarAccessoryViewController lives above all content in the native titlebar layer.
    if (_devToolsCloseButton) {
        [_devToolsCloseButton removeFromSuperview];
        _devToolsCloseButton = nil;
    }

    CGFloat btnSize = 24;
    CGFloat barHeight = 28;

    NSView* barView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 100, barHeight)];
    barView.wantsLayer = YES;
    barView.layer.backgroundColor = [DSColors devToolsBackground].CGColor;

    _devToolsCloseButton = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"xmark"
                                                                      accessibilityDescription:@"Close DevTools"]
                                              target:self
                                              action:@selector(closeDevTools)];
    _devToolsCloseButton.bordered = NO;
    _devToolsCloseButton.wantsLayer = YES;
    _devToolsCloseButton.layer.cornerRadius = btnSize / 2;
    _devToolsCloseButton.contentTintColor = [NSColor colorWithRed:0.6 green:0.6 blue:0.63 alpha:1.0];
    _devToolsCloseButton.frame = NSMakeRect(
        barView.bounds.size.width - btnSize - 6,
        (barHeight - btnSize) / 2,
        btnSize, btnSize);
    _devToolsCloseButton.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [barView addSubview:_devToolsCloseButton];

    NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
        initWithRect:_devToolsCloseButton.bounds
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:@{@"button": @"devToolsClose"}];
    [_devToolsCloseButton addTrackingArea:trackingArea];

    NSTitlebarAccessoryViewController* accessory = [[NSTitlebarAccessoryViewController alloc] init];
    accessory.view = barView;
    accessory.layoutAttribute = NSLayoutAttributeRight;

    while (_devToolsWindow.titlebarAccessoryViewControllers.count > 0) {
        [_devToolsWindow removeTitlebarAccessoryViewControllerAtIndex:0];
    }
    [_devToolsWindow addTitlebarAccessoryViewController:accessory];

    // Calculate positions
    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = kTitleBarHeight;
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

        // Make DevTools window key so it can receive mouse clicks
        // Child windows don't automatically become key when clicked
        [self->_devToolsWindow makeKeyWindow];
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
    CGFloat titleBarHeight = kTitleBarHeight;
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
    CGFloat titleBarHeight = kTitleBarHeight;
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
    CGFloat titleBarHeight = kTitleBarHeight;
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

- (void)resizeSidebarToWidth:(CGFloat)newWidth {
    // Ensure we're not collapsed
    if (_sidebarView.isCollapsed) return;

    _currentSidebarWidth = newWidth;

    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = kTitleBarHeight;

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
    CGFloat titleBarHeight = kTitleBarHeight;

    CGFloat newSidebarWidth = collapse ? kSidebarCollapsedWidth : _currentSidebarWidth;

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

#pragma mark - Account

- (void)showAccountPopover:(NSView*)anchorView {
    if (!_accountPopover) {
        _accountPopover = [[AccountPopoverController alloc] init];
        _accountPopover.windowController = self;
    }

    [_accountPopover showRelativeToView:anchorView];
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
    _eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event) {
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

- (void)dealloc {
    if (_hibernationTimer) {
        [_hibernationTimer invalidate];
        _hibernationTimer = nil;
    }
    if (_eventMonitor) {
        [NSEvent removeMonitor:_eventMonitor];
        _eventMonitor = nil;
    }
    if (_flagsChangedMonitor) {
        [NSEvent removeMonitor:_flagsChangedMonitor];
        _flagsChangedMonitor = nil;
    }
}

#pragma mark - NSWindowDelegate

- (void)shutdownAndQuit {
    if (_isShuttingDown) return;
    _isShuttingDown = YES;

    SaveSession();
    SessionStorage::MarkCleanShutdown();
    CefQuitMessageLoop();
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;

    // Hide window — app stays in dock (standard macOS behavior)
    SaveSession();
    SessionStorage::MarkCleanShutdown();
    [self.window orderOut:nil];
    return NO;
}

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    CefQuitMessageLoop();
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (_devToolsOpen && !_devToolsAnimating) {
        NSView* contentView = self.window.contentView;
        CGFloat titleBarHeight = kTitleBarHeight;
        CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;
        CGFloat sidebarWidth = _sidebarView.frame.size.width;
        CGFloat availableWidth = contentView.bounds.size.width - sidebarWidth;
        CGFloat browserWidth = availableWidth - _devToolsWidth - kDevToolsDividerWidth;

        _devToolsDivider.frame = NSMakeRect(sidebarWidth + browserWidth, kToolbarHeight, kDevToolsDividerWidth, browserHeight);

        // Update DevTools window position
        [self updateDevToolsWindowPosition];
    }
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    (void)notification;
    // If we were in content fullscreen mode and user exited via macOS controls,
    // restore the UI elements
    if (_isContentFullscreen) {
        _isContentFullscreen = NO;

        // Reset browser client's fullscreen state
        Tab* activeTab = _tabManager->GetActiveTab();
        if (activeTab && activeTab->client) {
            activeTab->client->SetContentFullscreen(false);
        }

        // Show UI elements
        _sidebarView.hidden = NO;
        _toolbarView.hidden = NO;
        _resizeHandle.hidden = NO;

        // Refresh sidebar panel visibility (ensures internal panels are shown)
        [_sidebarView showPanel:_sidebarView.activePanel];

        // Restore browser container to normal layout
        NSView* contentView = self.window.contentView;
        CGFloat titleBarHeight = kTitleBarHeight;
        CGFloat sidebarWidth = _currentSidebarWidth;
        CGFloat browserX = sidebarWidth;
        CGFloat browserY = kToolbarHeight;
        CGFloat browserWidth = contentView.bounds.size.width - sidebarWidth;
        CGFloat browserHeight = contentView.bounds.size.height - titleBarHeight - kToolbarHeight;

        _browserContainer.autoresizingMask = NSViewNotSizable;
        _browserContainer.frame = NSMakeRect(browserX, browserY, browserWidth, browserHeight);
        _browserContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // Resize browser views to fit container
        for (NSView* subview in _browserContainer.subviews) {
            if ([subview isKindOfClass:[NSView class]]) {
                subview.frame = _browserContainer.bounds;
            }
        }
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
