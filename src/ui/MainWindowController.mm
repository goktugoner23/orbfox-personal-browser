#import "MainWindowController.h"
#import "SidebarView.h"
#import "ToolbarView.h"
#import "Components.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "history_storage.h"

// Extern function to access global history storage
extern HistoryStorage* GetHistoryStorage();

static const CGFloat kSidebarWidth = 280.0;
static const CGFloat kToolbarHeight = 44.0;

@interface MainWindowController ()
@property (nonatomic, readwrite) TabManager* tabManager;
@property (nonatomic, readwrite) SidebarView* sidebarView;
@property (nonatomic, readwrite) ToolbarView* toolbarView;
@property (nonatomic, readwrite) NSView* browserContainer;
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
    _sidebarView = [[SidebarView alloc] initWithFrame:NSMakeRect(0, 0, kSidebarWidth, contentHeight)];
    _sidebarView.windowController = self;
    _sidebarView.autoresizingMask = NSViewHeightSizable;
    [contentView addSubview:_sidebarView];

    // Bottom toolbar
    _toolbarView = [[ToolbarView alloc] initWithFrame:NSMakeRect(kSidebarWidth, 0, bounds.size.width - kSidebarWidth, kToolbarHeight)];
    _toolbarView.windowController = self;
    _toolbarView.autoresizingMask = NSViewWidthSizable;
    [contentView addSubview:_toolbarView];

    // Browser container
    CGFloat browserX = kSidebarWidth;
    CGFloat browserY = kToolbarHeight;
    CGFloat browserWidth = bounds.size.width - kSidebarWidth;
    CGFloat browserHeight = contentHeight - kToolbarHeight;

    _browserContainer = [[NSView alloc] initWithFrame:NSMakeRect(browserX, browserY, browserWidth, browserHeight)];
    _browserContainer.wantsLayer = YES;
    _browserContainer.layer.backgroundColor = [NSColor blackColor].CGColor;
    _browserContainer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [contentView addSubview:_browserContainer];

    // Vertical divider between sidebar and content
    NSView* divider = [[NSView alloc] initWithFrame:NSMakeRect(kSidebarWidth - 1, 0, 1, bounds.size.height)];
    divider.wantsLayer = YES;
    divider.layer.backgroundColor = [NSColor colorWithRed:0.2 green:0.2 blue:0.22 alpha:1.0].CGColor;
    divider.autoresizingMask = NSViewHeightSizable;
    [contentView addSubview:divider];
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

        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf showBrowserWithRef:browser];
            [strongSelf.sidebarView selectTab:tabId];
            [strongSelf.toolbarView setURL:[NSString stringWithUTF8String:url.c_str()]];
        });
    };

    callbacks.on_tab_updated = [weakSelf](Tab* tab) {
        MainWindowController* strongSelf = weakSelf;
        if (!strongSelf || !tab) return;

        // Capture all data BEFORE dispatch_async to avoid dangling pointer
        int tabId = tab->id;
        std::string title = tab->title;
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

            // Update window title if this is the active tab
            Tab* activeTab = strongSelf.tabManager->GetActiveTab();
            if (activeTab && activeTab->id == tabId) {
                strongSelf.window.title = [NSString stringWithUTF8String:title.c_str()];
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
            if (!isLoading) {
                Tab* tab = strongSelf.tabManager->GetTabById(tabId);
                if (tab && !tab->url.empty() && tab->url.find("data:") != 0) {
                    HistoryStorage* history = GetHistoryStorage();
                    if (history) {
                        history->AddEntry(tab->url, tab->title);
                    }
                }
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                if (strongSelf.tabManager->GetActiveTab() &&
                    strongSelf.tabManager->GetActiveTab()->id == tabId) {
                    [strongSelf.toolbarView setCanGoBack:canGoBack canGoForward:canGoForward];
                    [strongSelf.toolbarView setLoading:isLoading];
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

#pragma mark - UI Updates

- (void)updateURLBar:(NSString*)url {
    [_toolbarView setURL:url];
}

- (void)updateNavigationButtons:(BOOL)canGoBack canGoForward:(BOOL)canGoForward {
    [_toolbarView setCanGoBack:canGoBack canGoForward:canGoForward];
}

- (void)focusURLBar {
    [_toolbarView focusURLField];
}

#pragma mark - Sidebar

static const CGFloat kIconStripWidth = 44.0;
static const CGFloat kSidebarExpandedWidth = 280.0;

- (void)toggleSidebarCollapse:(BOOL)collapse {
    NSView* contentView = self.window.contentView;
    CGFloat titleBarHeight = 28;

    CGFloat newSidebarWidth = collapse ? kIconStripWidth : kSidebarExpandedWidth;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = 0.2;
        context.allowsImplicitAnimation = YES;

        // Animate sidebar width
        NSRect sidebarFrame = _sidebarView.frame;
        sidebarFrame.size.width = newSidebarWidth;
        _sidebarView.animator.frame = sidebarFrame;

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

@end
