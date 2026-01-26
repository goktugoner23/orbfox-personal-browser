#import "SidebarView.h"
#import "MainWindowController.h"
#import "Components.h"
#include "history_storage.h"
#include "bookmark_storage.h"

// Extern function to access global storage
extern HistoryStorage* GetHistoryStorage();
extern BookmarkStorage* GetBookmarkStorage();

// Layout constants
static const CGFloat kIconStripWidth = 44.0;
static const CGFloat kSidebarWidth = 280.0;
static const CGFloat kWorkspaceHeight = 40.0;
static const CGFloat kNewTabButtonHeight = 44.0;

// ============================================================================
// FLIPPED VIEW
// For proper top-to-bottom layout in scroll views
// ============================================================================

@interface FlippedView : NSView
@end

@implementation FlippedView
- (BOOL)isFlipped { return YES; }
@end

// ============================================================================
// TAB ROW VIEW
// ============================================================================

@implementation TabRowView {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    DSIconButton* _closeButton;
    NSImageView* _pinIconView;
    NSProgressIndicator* _loadingIndicator;
    NSImageView* _faviconView;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        _isHovered = NO;
        _isSelected = NO;
        _isLoading = NO;
        _isPinned = NO;

        CGFloat iconSize = [DSLayout iconSizeSmall];
        CGFloat padding = [DSSpacing sm];

        // Favicon view
        _faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            padding + 2, (frame.size.height - iconSize) / 2, iconSize, iconSize)];
        _faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _faviconView.hidden = YES;
        [self addSubview:_faviconView];

        // Loading indicator
        _loadingIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(
            padding + 2, (frame.size.height - iconSize) / 2, iconSize, iconSize)];
        _loadingIndicator.style = NSProgressIndicatorStyleSpinning;
        _loadingIndicator.controlSize = NSControlSizeSmall;
        _loadingIndicator.displayedWhenStopped = NO;
        [self addSubview:_loadingIndicator];

        // Close button
        _closeButton = [DSIconButton buttonWithIcon:@"xmark"];
        _closeButton.frame = NSMakeRect(frame.size.width - 28, (frame.size.height - 20) / 2, 20, 20);

        // Pin icon (same position as close button - they swap based on hover)
        _pinIconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            frame.size.width - 25, (frame.size.height - 14) / 2, 14, 14)];
        _pinIconView.image = [NSImage imageWithSystemSymbolName:@"pin.fill" accessibilityDescription:@"Pinned"];
        _pinIconView.contentTintColor = [DSColors textSecondary];
        _pinIconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _pinIconView.autoresizingMask = NSViewMinXMargin;
        _pinIconView.hidden = YES;
        [self addSubview:_pinIconView];
        _closeButton.autoresizingMask = NSViewMinXMargin;
        _closeButton.hidden = YES;
        _closeButton.target = self;
        _closeButton.action = @selector(closeTab:);
        [self addSubview:_closeButton];
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
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow)
               owner:self
            userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    _isHovered = YES;
    _closeButton.hidden = NO;
    _pinIconView.hidden = YES;  // Hide pin icon, show close button on hover
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    _closeButton.hidden = YES;
    _pinIconView.hidden = !_isPinned;  // Show pin icon when not hovering (if pinned)
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    [_sidebarView.windowController activateTab:_tabId];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self showContextMenu:event];
}

- (void)showContextMenu:(NSEvent*)event {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Tab"];

    // Pin/Unpin option
    NSString* pinTitle = _isPinned ? @"Unpin Tab" : @"Pin Tab";
    NSMenuItem* pinItem = [[NSMenuItem alloc] initWithTitle:pinTitle
                                                     action:@selector(togglePinTab:)
                                              keyEquivalent:@""];
    pinItem.target = self;
    [menu addItem:pinItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeItem = [[NSMenuItem alloc] initWithTitle:@"Close Tab"
                                                       action:@selector(closeTab:)
                                                keyEquivalent:@""];
    closeItem.target = self;
    [menu addItem:closeItem];

    NSMenuItem* duplicateItem = [[NSMenuItem alloc] initWithTitle:@"Duplicate Tab"
                                                           action:@selector(duplicateTab:)
                                                    keyEquivalent:@""];
    duplicateItem.target = self;
    [menu addItem:duplicateItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* reloadItem = [[NSMenuItem alloc] initWithTitle:@"Reload Tab"
                                                        action:@selector(reloadTab:)
                                                 keyEquivalent:@""];
    reloadItem.target = self;
    [menu addItem:reloadItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeOthersItem = [[NSMenuItem alloc] initWithTitle:@"Close Other Tabs"
                                                             action:@selector(closeOtherTabs:)
                                                      keyEquivalent:@""];
    closeOthersItem.target = self;
    [menu addItem:closeOthersItem];

    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}

- (void)togglePinTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab) {
        tab->is_pinned = !tab->is_pinned;
        _isPinned = tab->is_pinned;
        _pinIconView.hidden = !_isPinned || _isHovered;  // Show pin icon only when pinned and not hovering
        [self setNeedsDisplay:YES];
    }
}

- (void)closeTab:(id)sender {
    (void)sender;
    [_sidebarView.windowController closeTab:_tabId];
}

- (void)duplicateTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab) {
        [_sidebarView.windowController createNewTab:[NSString stringWithUTF8String:tab->url.c_str()]];
    }
}

- (void)reloadTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab && tab->browser) {
        tab->browser->Reload();
    }
}

- (void)closeOtherTabs:(id)sender {
    (void)sender;
    Workspace* workspace = _sidebarView.windowController.tabManager->GetActiveWorkspace();
    if (!workspace) return;

    std::vector<int> tabsToClose;
    for (const auto& tab : workspace->tabs) {
        if (tab->id != _tabId) {
            tabsToClose.push_back(tab->id);
        }
    }

    for (int tabId : tabsToClose) {
        [_sidebarView.windowController closeTab:tabId];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    NSColor* bgColor = nil;
    if (_isSelected) {
        bgColor = [DSColors surfaceActive];
    } else if (_isHovered) {
        bgColor = [DSColors surfaceHover];
    }

    if (bgColor) {
        [bgColor setFill];
        CGFloat inset = [DSSpacing xs];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, inset, 2)
                                                             xRadius:[DSLayout cornerRadiusMedium]
                                                             yRadius:[DSLayout cornerRadiusMedium]];
        [path fill];
    }

    // Draw title
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    style.lineBreakMode = NSLineBreakByTruncatingTail;

    NSDictionary* attrs = [DSTypography attributesWithStyle:DSFontStyleBody
                                                      color:[DSColors textPrimary]];
    NSMutableDictionary* mutableAttrs = [attrs mutableCopy];
    mutableAttrs[NSParagraphStyleAttributeName] = style;

    BOOL hasIcon = _isLoading || (_favicon != nil);
    CGFloat titleX = hasIcon ? 32 : [DSSpacing md];
    NSRect titleRect = NSMakeRect(titleX, 10, self.bounds.size.width - titleX - 32, 18);
    [_title drawInRect:titleRect withAttributes:mutableAttrs];
}

- (void)setTitle:(NSString*)title {
    _title = [title copy];
    [self setNeedsDisplay:YES];
}

- (void)setIsSelected:(BOOL)isSelected {
    _isSelected = isSelected;
    [self setNeedsDisplay:YES];
}

- (void)setIsLoading:(BOOL)isLoading {
    _isLoading = isLoading;
    if (isLoading) {
        [_loadingIndicator startAnimation:nil];
        _faviconView.hidden = YES;
    } else {
        [_loadingIndicator stopAnimation:nil];
        _faviconView.hidden = (_favicon == nil);
    }
    [self setNeedsDisplay:YES];
}

- (void)setFavicon:(NSImage*)favicon {
    _favicon = favicon;
    _faviconView.image = favicon;
    _faviconView.hidden = (_isLoading || favicon == nil);
    [self setNeedsDisplay:YES];
}

- (void)setIsPinned:(BOOL)isPinned {
    _isPinned = isPinned;
    _pinIconView.hidden = !isPinned || _isHovered;  // Show pin icon only when pinned and not hovering
    [self setNeedsDisplay:YES];
}

@end

// ============================================================================
// SIDEBAR VIEW
// ============================================================================

@implementation SidebarView {
    NSView* _iconStrip;

    // Tabs panel
    NSView* _tabsPanelContainer;
    NSView* _workspaceSelector;
    NSScrollView* _workspaceScrollView;  // Scrollable workspace tabs
    NSView* _workspaceTabsContainer;
    NSScrollView* _tabScrollView;
    FlippedView* _tabContainer;
    DSButton* _newTabButton;
    DSIconButton* _addWorkspaceBtn;
    NSMutableArray<DSButton*>* _workspaceTabs;  // Array of workspace tab buttons

    // History panel
    NSView* _historyPanelContainer;
    NSScrollView* _historyScrollView;
    FlippedView* _historyContainer;

    // Bookmarks panel
    NSView* _bookmarksPanelContainer;
    NSScrollView* _bookmarksScrollView;
    FlippedView* _bookmarksContainer;

    // Icon buttons
    DSIconButton* _tabsIcon;
    DSIconButton* _favoritesIcon;
    DSIconButton* _historyIcon;
    DSIconButton* _downloadsIcon;

    NSMutableArray<TabRowView*>* _tabRows;
    BOOL _isCollapsed;
}

@synthesize isCollapsed = _isCollapsed;

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _activePanel = SidebarPanelTabs;
        _tabRows = [NSMutableArray array];
        _workspaceTabs = [NSMutableArray array];
        [self setupViews];
    }
    return self;
}

- (void)setupViews {
    // Icon strip
    _iconStrip = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kIconStripWidth, self.bounds.size.height)];
    _iconStrip.wantsLayer = YES;
    _iconStrip.layer.backgroundColor = [DSColors backgroundSecondary].CGColor;
    _iconStrip.autoresizingMask = NSViewHeightSizable;
    [self addSubview:_iconStrip];

    // Icon buttons
    CGFloat iconY = self.bounds.size.height - 50;
    CGFloat iconSize = [DSLayout iconSizeLarge];
    CGFloat iconX = (kIconStripWidth - iconSize) / 2;

    _tabsIcon = [self createIconButton:@"square.on.square" y:iconY tooltip:@"Tabs"];
    _tabsIcon.tag = SidebarPanelTabs;
    [_iconStrip addSubview:_tabsIcon];

    iconY -= 40;
    _favoritesIcon = [self createIconButton:@"bookmark" y:iconY tooltip:@"Bookmarks"];
    _favoritesIcon.tag = SidebarPanelFavorites;
    [_iconStrip addSubview:_favoritesIcon];

    iconY -= 40;
    _historyIcon = [self createIconButton:@"clock" y:iconY tooltip:@"History"];
    _historyIcon.tag = SidebarPanelHistory;
    [_iconStrip addSubview:_historyIcon];

    iconY -= 40;
    _downloadsIcon = [self createIconButton:@"arrow.down.circle" y:iconY tooltip:@"Downloads"];
    _downloadsIcon.tag = SidebarPanelDownloads;
    [_iconStrip addSubview:_downloadsIcon];

    // Content area
    CGFloat contentX = kIconStripWidth;
    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;
    CGFloat contentHeight = self.bounds.size.height;

    [self setupTabsPanel:contentX width:contentWidth height:contentHeight];
    [self setupBookmarksPanel:contentX width:contentWidth height:contentHeight];
    [self setupHistoryPanel:contentX width:contentWidth height:contentHeight];

    [self updatePanelVisibility];

    // Initial load of workspace tabs (will be populated when windowController is set)
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) {
        // Update icon selection when view is added to window
        [self updateIconSelection];
    }
}

- (DSIconButton*)createIconButton:(NSString*)symbolName y:(CGFloat)y tooltip:(NSString*)tooltip {
    CGFloat iconSize = [DSLayout iconSizeLarge];
    CGFloat iconX = (kIconStripWidth - iconSize) / 2;

    DSIconButton* btn = [DSIconButton buttonWithIcon:symbolName tooltip:tooltip];
    btn.frame = NSMakeRect(iconX, y, iconSize, iconSize);
    btn.showsHoverBackground = NO;  // Only selected icon shows background
    btn.target = self;
    btn.action = @selector(iconClicked:);
    btn.autoresizingMask = NSViewMinYMargin;
    return btn;
}

- (void)setupTabsPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _tabsPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _tabsPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _tabsPanelContainer.wantsLayer = YES;
    _tabsPanelContainer.layer.masksToBounds = YES;  // Clip contents when collapsed
    [self addSubview:_tabsPanelContainer];

    CGFloat padding = [DSSpacing sm];
    CGFloat btnSize = 28;

    // Workspace selector row at top
    _workspaceSelector = [[NSView alloc] initWithFrame:NSMakeRect(
        0, height - kWorkspaceHeight - padding, width, kWorkspaceHeight)];
    _workspaceSelector.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [_tabsPanelContainer addSubview:_workspaceSelector];

    // Add workspace button - on the RIGHT side with padding
    _addWorkspaceBtn = [DSIconButton buttonWithIcon:@"plus" tooltip:@"New Workspace"];
    _addWorkspaceBtn.frame = NSMakeRect(width - btnSize - padding, (kWorkspaceHeight - btnSize) / 2, btnSize, btnSize);
    _addWorkspaceBtn.autoresizingMask = NSViewMinXMargin;
    _addWorkspaceBtn.target = self;
    _addWorkspaceBtn.action = @selector(addWorkspaceClicked:);
    [_workspaceSelector addSubview:_addWorkspaceBtn];

    // Workspace tabs scroll view - horizontal scrolling for workspace tabs
    CGFloat scrollWidth = width - btnSize - padding * 2;
    _workspaceScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, scrollWidth, kWorkspaceHeight)];
    _workspaceScrollView.hasHorizontalScroller = YES;
    _workspaceScrollView.hasVerticalScroller = NO;
    _workspaceScrollView.horizontalScroller.alphaValue = 0;  // Hide scrollbar but keep functionality
    _workspaceScrollView.drawsBackground = NO;
    _workspaceScrollView.autoresizingMask = NSViewWidthSizable;
    _workspaceScrollView.horizontalScrollElasticity = NSScrollElasticityAllowed;
    [_workspaceSelector addSubview:_workspaceScrollView];

    // Container for workspace tab buttons
    _workspaceTabsContainer = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, scrollWidth, kWorkspaceHeight)];
    _workspaceScrollView.documentView = _workspaceTabsContainer;

    // New Tab button at bottom - full width, icon on right
    _newTabButton = [DSButton buttonWithTitle:@"New Tab" icon:@"plus" variant:DSButtonVariantGhost];
    _newTabButton.frame = NSMakeRect(padding, padding, width - padding * 2, 34);
    _newTabButton.imagePosition = NSImageTrailing;  // Icon on the right
    _newTabButton.autoresizingMask = NSViewMaxYMargin | NSViewWidthSizable;
    _newTabButton.target = self;
    _newTabButton.action = @selector(newTabClicked:);
    [_tabsPanelContainer addSubview:_newTabButton];

    // Tab scroll view - between workspace selector and new tab button
    CGFloat tabAreaTop = height - kWorkspaceHeight - [DSSpacing lg];
    CGFloat tabAreaBottom = kNewTabButtonHeight;
    CGFloat tabAreaHeight = tabAreaTop - tabAreaBottom;

    _tabScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, tabAreaBottom, width, tabAreaHeight)];
    _tabScrollView.hasVerticalScroller = YES;
    _tabScrollView.hasHorizontalScroller = NO;
    _tabScrollView.autohidesScrollers = YES;
    _tabScrollView.drawsBackground = NO;
    _tabScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_tabsPanelContainer addSubview:_tabScrollView];

    _tabContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, tabAreaHeight)];
    _tabScrollView.documentView = _tabContainer;
}

// Called when sidebar width changes (from resize)
- (void)updateLayoutForWidth:(CGFloat)newWidth {
    CGFloat contentWidth = newWidth - kIconStripWidth;

    // Update tab container width
    NSRect containerFrame = _tabContainer.frame;
    containerFrame.size.width = contentWidth;
    _tabContainer.frame = containerFrame;

    // Update bookmarks container width
    NSRect bookmarksFrame = _bookmarksContainer.frame;
    bookmarksFrame.size.width = contentWidth;
    _bookmarksContainer.frame = bookmarksFrame;

    // Update history container width
    NSRect historyFrame = _historyContainer.frame;
    historyFrame.size.width = contentWidth;
    _historyContainer.frame = historyFrame;

    // Reload tabs to update row widths
    [self reloadTabs];
}

- (void)setupBookmarksPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _bookmarksPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _bookmarksPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _bookmarksPanelContainer.wantsLayer = YES;
    _bookmarksPanelContainer.layer.masksToBounds = YES;
    _bookmarksPanelContainer.hidden = YES;
    [self addSubview:_bookmarksPanelContainer];

    // Bookmarks title
    NSTextField* bookmarksTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(
        [DSSpacing md], height - 40, width - [DSSpacing xl] - 32, 24)];
    bookmarksTitle.stringValue = @"Bookmarks";
    bookmarksTitle.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    bookmarksTitle.textColor = [DSColors textPrimary];
    bookmarksTitle.bezeled = NO;
    bookmarksTitle.drawsBackground = NO;
    bookmarksTitle.editable = NO;
    bookmarksTitle.selectable = NO;
    bookmarksTitle.autoresizingMask = NSViewMinYMargin;
    [_bookmarksPanelContainer addSubview:bookmarksTitle];

    // Add bookmark button (star icon)
    DSIconButton* addBookmarkBtn = [DSIconButton buttonWithIcon:@"plus" tooltip:@"Bookmark this page"];
    addBookmarkBtn.frame = NSMakeRect(width - 36, height - 40, 28, 28);
    addBookmarkBtn.autoresizingMask = NSViewMinYMargin | NSViewMinXMargin;
    addBookmarkBtn.target = self;
    addBookmarkBtn.action = @selector(addBookmarkClicked:);
    [_bookmarksPanelContainer addSubview:addBookmarkBtn];

    // Bookmarks scroll view
    _bookmarksScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, width, height - 50)];
    _bookmarksScrollView.hasVerticalScroller = YES;
    _bookmarksScrollView.hasHorizontalScroller = NO;
    _bookmarksScrollView.autohidesScrollers = YES;
    _bookmarksScrollView.drawsBackground = NO;
    _bookmarksScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_bookmarksPanelContainer addSubview:_bookmarksScrollView];

    _bookmarksContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, height - 50)];
    _bookmarksScrollView.documentView = _bookmarksContainer;
}

- (void)setupHistoryPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _historyPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _historyPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _historyPanelContainer.wantsLayer = YES;
    _historyPanelContainer.layer.masksToBounds = YES;  // Clip contents when collapsed
    _historyPanelContainer.hidden = YES;
    [self addSubview:_historyPanelContainer];

    // History title
    NSTextField* historyTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(
        [DSSpacing md], height - 40, width - [DSSpacing xl], 24)];
    historyTitle.stringValue = @"History";
    historyTitle.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    historyTitle.textColor = [DSColors textPrimary];
    historyTitle.bezeled = NO;
    historyTitle.drawsBackground = NO;
    historyTitle.editable = NO;
    historyTitle.selectable = NO;
    historyTitle.autoresizingMask = NSViewMinYMargin;
    [_historyPanelContainer addSubview:historyTitle];

    // History scroll view
    _historyScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, width, height - 50)];
    _historyScrollView.hasVerticalScroller = YES;
    _historyScrollView.hasHorizontalScroller = NO;
    _historyScrollView.autohidesScrollers = YES;
    _historyScrollView.drawsBackground = NO;
    _historyScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_historyPanelContainer addSubview:_historyScrollView];

    _historyContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, height - 50)];
    _historyScrollView.documentView = _historyContainer;
}

#pragma mark - Actions

- (void)iconClicked:(NSButton*)sender {
    SidebarPanel clickedPanel = (SidebarPanel)sender.tag;

    if (clickedPanel == _activePanel) {
        [self toggleSidebar];
    } else {
        if (_isCollapsed) {
            [self toggleSidebar];
        }
        _activePanel = clickedPanel;
        [self updateIconSelection];
        [self updatePanelVisibility];
    }
}

- (void)toggleSidebar {
    _isCollapsed = !_isCollapsed;
    [_windowController toggleSidebarCollapse:_isCollapsed];
}

- (void)showPanel:(SidebarPanel)panel {
    if (_isCollapsed) {
        [self toggleSidebar];
    }
    _activePanel = panel;
    [self updateIconSelection];
    [self updatePanelVisibility];
}

- (void)newTabClicked:(id)sender {
    (void)sender;
    [_windowController createNewTab:@""];
}

- (void)addWorkspaceClicked:(id)sender {
    (void)sender;
    if (!_windowController || !_windowController.tabManager) return;

    // Count existing workspaces to generate name (WS 1, WS 2, etc.)
    size_t count = _windowController.tabManager->GetWorkspaces().size();
    NSString* spaceName = [NSString stringWithFormat:@"WS %zu", count + 1];

    // Create new workspace
    Workspace* newWorkspace = _windowController.tabManager->CreateWorkspace([spaceName UTF8String]);
    if (!newWorkspace) return;

    // Switch to the new workspace
    _windowController.tabManager->SetActiveWorkspace(newWorkspace->id);

    // Update UI
    [self reloadWorkspaceTabs];
    [self reloadTabs];

    // Create initial tab in new workspace
    [_windowController createNewTab:@""];
}

- (void)workspaceTabClicked:(id)sender {
    if (!_windowController || !_windowController.tabManager) return;

    int workspaceId = (int)[sender tag];

    // Don't switch if already on this workspace
    Workspace* currentWorkspace = _windowController.tabManager->GetActiveWorkspace();
    if (currentWorkspace && currentWorkspace->id == workspaceId) return;

    _windowController.tabManager->SetActiveWorkspace(workspaceId);

    [self reloadWorkspaceTabs];
    [self reloadTabs];

    // Show the active tab's browser
    Tab* activeTab = _windowController.tabManager->GetActiveTab();
    if (activeTab) {
        [_windowController activateTab:activeTab->id];
    } else {
        // Create a tab if workspace is empty
        [_windowController createNewTab:@""];
    }
}

- (void)reloadWorkspaceTabs {
    if (!_windowController || !_windowController.tabManager) return;

    // Remove existing workspace tab buttons
    for (NSView* view in _workspaceTabs) {
        [view removeFromSuperview];
    }
    [_workspaceTabs removeAllObjects];

    const auto& workspaces = _windowController.tabManager->GetWorkspaces();
    Workspace* activeWorkspace = _windowController.tabManager->GetActiveWorkspace();

    CGFloat padding = [DSSpacing sm];
    CGFloat tabHeight = 32;
    CGFloat x = padding;
    CGFloat closeSize = 16;

    for (const auto& workspace : workspaces) {
        BOOL isActive = (activeWorkspace && workspace->id == activeWorkspace->id);
        int workspaceId = workspace->id;

        // Create container view for the workspace tab
        NSView* tabContainer = [[NSView alloc] init];
        tabContainer.wantsLayer = YES;
        tabContainer.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        tabContainer.layer.backgroundColor = isActive ? [DSColors surfaceActive].CGColor : [NSColor clearColor].CGColor;

        // Calculate total width: padding + text + gap + close button + padding
        NSString* title = [NSString stringWithUTF8String:workspace->name.c_str()];
        NSDictionary* attrs = @{NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightMedium]};
        CGFloat textWidth = [title sizeWithAttributes:attrs].width;
        CGFloat innerPadding = 10;
        CGFloat totalWidth = innerPadding + textWidth + 6 + closeSize + innerPadding;
        totalWidth = MAX(70, totalWidth);

        tabContainer.frame = NSMakeRect(x, (kWorkspaceHeight - tabHeight) / 2, totalWidth, tabHeight);

        // Title label
        NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(
            innerPadding, (tabHeight - 18) / 2, textWidth + 4, 18)];
        label.stringValue = title;
        label.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
        label.textColor = [DSColors textPrimary];
        label.bezeled = NO;
        label.drawsBackground = NO;
        label.editable = NO;
        label.selectable = NO;
        [tabContainer addSubview:label];

        // Close button inside the tab (always visible)
        DSIconButton* closeBtn = [DSIconButton buttonWithIcon:@"xmark"];
        closeBtn.frame = NSMakeRect(totalWidth - innerPadding - closeSize, (tabHeight - closeSize) / 2, closeSize, closeSize);
        closeBtn.tag = workspaceId;
        closeBtn.target = self;
        closeBtn.action = @selector(closeWorkspace:);
        closeBtn.alphaValue = 0.5;
        [tabContainer addSubview:closeBtn];

        // Make the container clickable (excluding close button area)
        NSButton* clickArea = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, totalWidth - closeSize - 6, tabHeight)];
        clickArea.transparent = YES;
        clickArea.bordered = NO;
        clickArea.tag = workspaceId;
        clickArea.target = self;
        clickArea.action = @selector(workspaceTabClicked:);
        [tabContainer addSubview:clickArea positioned:NSWindowBelow relativeTo:nil];

        // Add right-click menu to container (delete always enabled)
        NSMenu* contextMenu = [self createWorkspaceContextMenu:workspaceId canDelete:YES];
        tabContainer.menu = contextMenu;

        [_workspaceTabsContainer addSubview:tabContainer];
        [_workspaceTabs addObject:tabContainer];

        x += totalWidth + [DSSpacing xs];
    }

    // Update container width to fit all tabs
    NSRect containerFrame = _workspaceTabsContainer.frame;
    containerFrame.size.width = MAX(x, _workspaceScrollView.bounds.size.width);
    _workspaceTabsContainer.frame = containerFrame;
}

- (NSMenu*)createWorkspaceContextMenu:(int)workspaceId canDelete:(BOOL)canDelete {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Workspace"];

    // Rename
    NSMenuItem* renameItem = [[NSMenuItem alloc] initWithTitle:@"Rename Space"
                                                         action:@selector(renameWorkspace:)
                                                  keyEquivalent:@""];
    renameItem.target = self;
    renameItem.tag = workspaceId;
    [menu addItem:renameItem];

    // Duplicate
    NSMenuItem* duplicateItem = [[NSMenuItem alloc] initWithTitle:@"Duplicate Space"
                                                            action:@selector(duplicateWorkspace:)
                                                     keyEquivalent:@""];
    duplicateItem.target = self;
    duplicateItem.tag = workspaceId;
    [menu addItem:duplicateItem];

    [menu addItem:[NSMenuItem separatorItem]];

    // Delete (only if more than 1 workspace)
    NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete Space"
                                                         action:canDelete ? @selector(deleteWorkspace:) : nil
                                                  keyEquivalent:@""];
    deleteItem.target = self;
    deleteItem.tag = workspaceId;
    if (!canDelete) {
        deleteItem.enabled = NO;
    }
    [menu addItem:deleteItem];

    return menu;
}

- (void)closeWorkspace:(DSIconButton*)sender {
    [self deleteWorkspaceById:(int)sender.tag];
}

- (void)deleteWorkspace:(NSMenuItem*)sender {
    [self deleteWorkspaceById:(int)sender.tag];
}

- (void)deleteWorkspaceById:(int)workspaceId {
    if (!_windowController || !_windowController.tabManager) return;

    // If this is the last workspace, quit the app
    if (_windowController.tabManager->GetWorkspaces().size() <= 1) {
        [_windowController.window close];
        return;
    }

    // Check for pinned tabs in this workspace
    BOOL hasPinnedTabs = NO;
    Workspace* workspaceToDelete = nullptr;
    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        if (workspace->id == workspaceId) {
            workspaceToDelete = workspace.get();
            for (const auto& tab : workspace->tabs) {
                if (tab->is_pinned) {
                    hasPinnedTabs = YES;
                    break;
                }
            }
            break;
        }
    }

    if (hasPinnedTabs) {
        // Show warning dialog
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"Close Space with Pinned Tabs?";
        alert.informativeText = @"This space contains pinned tabs. Are you sure you want to close it?";
        [alert addButtonWithTitle:@"Close Space"];
        [alert addButtonWithTitle:@"Cancel"];
        alert.alertStyle = NSAlertStyleWarning;

        [alert beginSheetModalForWindow:_windowController.window completionHandler:^(NSModalResponse response) {
            if (response == NSAlertFirstButtonReturn) {
                [self performWorkspaceDeletion:workspaceId];
            }
        }];
    } else {
        [self performWorkspaceDeletion:workspaceId];
    }
}

- (void)performWorkspaceDeletion:(int)workspaceId {
    // Find the workspace and close its browser views
    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        if (workspace->id == workspaceId) {
            for (const auto& tab : workspace->tabs) {
                if (tab->browser) {
                    // Remove the browser view from superview first
                    CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
                    if (host) {
                        NSView* browserView = (__bridge NSView*)host->GetWindowHandle();
                        if (browserView) {
                            [browserView removeFromSuperview];
                        }
                        // Now close the browser
                        host->CloseBrowser(true);
                    }
                }
            }
            break;
        }
    }

    // Delete the workspace from tab manager
    _windowController.tabManager->DeleteWorkspace(workspaceId);

    // Update UI
    [self reloadWorkspaceTabs];
    [self reloadTabs];

    // Show the active workspace's active tab
    Tab* activeTab = _windowController.tabManager->GetActiveTab();
    if (activeTab && activeTab->browser) {
        [_windowController activateTab:activeTab->id];
    } else {
        // If no active tab, create one in the current workspace
        Workspace* activeWorkspace = _windowController.tabManager->GetActiveWorkspace();
        if (activeWorkspace && activeWorkspace->tabs.empty()) {
            [_windowController createNewTab:@""];
        }
    }
}

- (void)renameWorkspace:(NSMenuItem*)sender {
    int workspaceId = (int)sender.tag;
    if (!_windowController || !_windowController.tabManager) return;

    // Find the workspace
    Workspace* workspace = nullptr;
    for (const auto& ws : _windowController.tabManager->GetWorkspaces()) {
        if (ws->id == workspaceId) {
            workspace = ws.get();
            break;
        }
    }
    if (!workspace) return;

    // Show rename alert
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Rename Space";
    alert.informativeText = @"Enter a new name for this space:";
    [alert addButtonWithTitle:@"Rename"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    input.stringValue = [NSString stringWithUTF8String:workspace->name.c_str()];
    alert.accessoryView = input;

    [alert beginSheetModalForWindow:_windowController.window completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            NSString* newName = input.stringValue;
            if (newName.length > 0) {
                workspace->name = [newName UTF8String];
                [self reloadWorkspaceTabs];
            }
        }
    }];
}

- (void)duplicateWorkspace:(NSMenuItem*)sender {
    int workspaceId = (int)sender.tag;
    if (!_windowController || !_windowController.tabManager) return;

    // Find the workspace to duplicate
    Workspace* sourceWorkspace = nullptr;
    for (const auto& ws : _windowController.tabManager->GetWorkspaces()) {
        if (ws->id == workspaceId) {
            sourceWorkspace = ws.get();
            break;
        }
    }
    if (!sourceWorkspace) return;

    // Create new workspace with copied name
    size_t count = _windowController.tabManager->GetWorkspaces().size();
    NSString* newName = [NSString stringWithFormat:@"%s Copy", sourceWorkspace->name.c_str()];

    Workspace* newWorkspace = _windowController.tabManager->CreateWorkspace([newName UTF8String]);
    if (!newWorkspace) return;

    // Copy tabs (create new tabs with same URLs)
    _windowController.tabManager->SetActiveWorkspace(newWorkspace->id);

    for (const auto& tab : sourceWorkspace->tabs) {
        [_windowController createNewTab:[NSString stringWithUTF8String:tab->url.c_str()]];
    }

    // If no tabs were copied, create a default one
    if (sourceWorkspace->tabs.empty()) {
        [_windowController createNewTab:@""];
    }

    [self reloadWorkspaceTabs];
    [self reloadTabs];
}

- (void)updateWorkspaceButton {
    // Now we just reload the workspace tabs instead
    [self reloadWorkspaceTabs];
}

#pragma mark - UI Updates

- (void)updatePanelVisibility {
    _tabsPanelContainer.hidden = YES;
    _bookmarksPanelContainer.hidden = YES;
    _historyPanelContainer.hidden = YES;

    switch (_activePanel) {
        case SidebarPanelTabs:
            _tabsPanelContainer.hidden = NO;
            break;
        case SidebarPanelFavorites:  // Bookmarks panel
            _bookmarksPanelContainer.hidden = NO;
            [self reloadBookmarks];
            break;
        case SidebarPanelHistory:
            _historyPanelContainer.hidden = NO;
            [self reloadHistory];
            break;
        default:
            break;
    }
}

- (void)updateIconSelection {
    // Set selected state - selected icon shows persistent hover background
    _tabsIcon.selected = (_activePanel == SidebarPanelTabs);
    _favoritesIcon.selected = (_activePanel == SidebarPanelFavorites);
    _historyIcon.selected = (_activePanel == SidebarPanelHistory);
    _downloadsIcon.selected = (_activePanel == SidebarPanelDownloads);
}

#pragma mark - Tabs

- (void)reloadTabs {
    for (TabRowView* row in _tabRows) {
        [row removeFromSuperview];
    }
    [_tabRows removeAllObjects];

    if (!_windowController || !_windowController.tabManager) return;

    // Also reload workspace tabs if needed
    if (_workspaceTabs.count == 0) {
        [self reloadWorkspaceTabs];
    }

    Workspace* workspace = _windowController.tabManager->GetActiveWorkspace();
    if (!workspace) return;

    // Use current sidebar width instead of constant
    CGFloat contentWidth = self.bounds.size.width - kIconStripWidth;
    CGFloat rowHeight = [DSLayout rowHeight];
    CGFloat totalHeight = workspace->tabs.size() * rowHeight;
    CGFloat minHeight = _tabScrollView.bounds.size.height;

    _tabContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(totalHeight, minHeight));

    CGFloat y = 0;
    int activeTabId = workspace->GetActiveTab() ? workspace->GetActiveTab()->id : -1;

    for (const auto& tab : workspace->tabs) {
        TabRowView* row = [[TabRowView alloc] initWithFrame:NSMakeRect(0, y, contentWidth, rowHeight)];
        row.tabId = tab->id;
        row.title = [NSString stringWithUTF8String:tab->title.c_str()];
        row.isSelected = (tab->id == activeTabId);
        row.isLoading = tab->is_loading;
        row.isPinned = tab->is_pinned;
        row.sidebarView = self;

        if (!tab->favicon_data.empty()) {
            NSData* faviconData = [NSData dataWithBytes:tab->favicon_data.data()
                                                 length:tab->favicon_data.size()];
            NSImage* favicon = [[NSImage alloc] initWithData:faviconData];
            if (favicon) {
                row.favicon = favicon;
            }
        }

        [_tabContainer addSubview:row];
        [_tabRows addObject:row];
        y += rowHeight;
    }
}

- (void)selectTab:(int)tabId {
    for (TabRowView* row in _tabRows) {
        row.isSelected = (row.tabId == tabId);
    }
}

- (void)updateTab:(int)tabId title:(NSString*)title isLoading:(BOOL)isLoading {
    for (TabRowView* row in _tabRows) {
        if (row.tabId == tabId) {
            row.title = title;
            row.isLoading = isLoading;
            break;
        }
    }
}

- (void)updateTab:(int)tabId faviconData:(NSData*)faviconData {
    if (!faviconData || faviconData.length == 0) return;

    NSImage* favicon = [[NSImage alloc] initWithData:faviconData];
    if (!favicon) return;

    for (TabRowView* row in _tabRows) {
        if (row.tabId == tabId) {
            row.favicon = favicon;
            break;
        }
    }
}

#pragma mark - Bookmarks

- (void)addBookmarkClicked:(id)sender {
    (void)sender;
    if (!_windowController) return;

    // Get current tab's URL and title
    Tab* activeTab = _windowController.tabManager->GetActiveTab();
    if (!activeTab) return;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    NSString* url = [NSString stringWithUTF8String:activeTab->url.c_str()];
    NSString* title = [NSString stringWithUTF8String:activeTab->title.c_str()];

    // Toggle bookmark - remove if exists, add if not
    if (bookmarks->IsBookmarked(activeTab->url)) {
        bookmarks->DeleteBookmarkByUrl(activeTab->url);
    } else {
        bookmarks->AddBookmark(activeTab->url, activeTab->title);
    }

    [self reloadBookmarks];
}

- (void)reloadBookmarks {
    for (NSView* subview in _bookmarksContainer.subviews.copy) {
        [subview removeFromSuperview];
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    std::vector<Bookmark> entries = bookmarks->GetAllBookmarks();
    CGFloat contentWidth = _bookmarksContainer.bounds.size.width;
    CGFloat y = 0;
    CGFloat rowHeight = 44;

    if (entries.empty()) {
        // Show empty state
        NSTextField* emptyLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
            [DSSpacing md], y + 20, contentWidth - [DSSpacing xl], 40)];
        emptyLabel.stringValue = @"No bookmarks yet.\nClick + to bookmark the current page.";
        emptyLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
        emptyLabel.textColor = [DSColors textSecondary];
        emptyLabel.bezeled = NO;
        emptyLabel.drawsBackground = NO;
        emptyLabel.editable = NO;
        emptyLabel.selectable = NO;
        emptyLabel.alignment = NSTextAlignmentCenter;
        [_bookmarksContainer addSubview:emptyLabel];
        return;
    }

    for (const auto& entry : entries) {
        // Bookmark row using DSRow
        DSRow* row = [[DSRow alloc] initWithFrame:NSMakeRect(
            [DSSpacing xs], y, contentWidth - [DSSpacing sm], rowHeight)];

        NSString* title = [NSString stringWithUTF8String:entry.title.empty()
            ? entry.url.c_str() : entry.title.c_str()];
        NSString* urlStr = [NSString stringWithUTF8String:entry.url.c_str()];

        row.title = title;
        row.showsCloseButton = YES;

        __weak SidebarView* weakSelf = self;
        NSString* urlCopy = urlStr;
        int64_t bookmarkId = entry.id;

        row.onClick = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            [strongSelf.windowController navigateToURL:urlCopy];
            strongSelf->_activePanel = SidebarPanelTabs;
            [strongSelf updateIconSelection];
            [strongSelf updatePanelVisibility];
        };

        row.onClose = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            BookmarkStorage* bm = GetBookmarkStorage();
            if (bm) {
                bm->DeleteBookmark(bookmarkId);
                [strongSelf reloadBookmarks];
            }
        };

        [_bookmarksContainer addSubview:row];
        y += rowHeight;
    }

    _bookmarksContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _bookmarksScrollView.bounds.size.height));
}

#pragma mark - History

- (void)reloadHistory {
    for (NSView* subview in _historyContainer.subviews.copy) {
        [subview removeFromSuperview];
    }

    HistoryStorage* history = GetHistoryStorage();
    if (!history) return;

    std::vector<HistoryEntry> entries = history->GetRecentHistory(100);
    CGFloat contentWidth = _historyContainer.bounds.size.width;
    CGFloat y = 0;
    CGFloat rowHeight = 48;

    NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    dateFormatter.timeStyle = NSDateFormatterNoStyle;

    NSDateFormatter* timeFormatter = [[NSDateFormatter alloc] init];
    timeFormatter.dateStyle = NSDateFormatterNoStyle;
    timeFormatter.timeStyle = NSDateFormatterShortStyle;

    NSCalendar* calendar = [NSCalendar currentCalendar];
    NSDate* today = [calendar startOfDayForDate:[NSDate date]];
    NSDate* yesterday = [calendar dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:today options:0];

    NSString* lastDateString = nil;

    for (const auto& entry : entries) {
        NSDate* visitDate = [NSDate dateWithTimeIntervalSince1970:entry.visit_time];
        NSDate* dayStart = [calendar startOfDayForDate:visitDate];

        NSString* dateString;
        if ([dayStart isEqualToDate:today]) {
            dateString = @"Today";
        } else if ([dayStart isEqualToDate:yesterday]) {
            dateString = @"Yesterday";
        } else {
            dateString = [dateFormatter stringFromDate:visitDate];
        }

        // Date header
        if (![dateString isEqualToString:lastDateString]) {
            NSTextField* dateHeader = [[NSTextField alloc] initWithFrame:NSMakeRect(
                [DSSpacing md], y, contentWidth - [DSSpacing xl], 24)];
            dateHeader.stringValue = dateString;
            dateHeader.font = [DSTypography fontWithStyle:DSFontStyleCaptionMedium];
            dateHeader.textColor = [DSColors textSecondary];
            dateHeader.bezeled = NO;
            dateHeader.drawsBackground = NO;
            dateHeader.editable = NO;
            dateHeader.selectable = NO;
            [_historyContainer addSubview:dateHeader];
            y += 28;
            lastDateString = dateString;
        }

        // History row using DSHistoryRow
        DSHistoryRow* row = [[DSHistoryRow alloc] initWithFrame:NSMakeRect(
            [DSSpacing xs], y, contentWidth - [DSSpacing sm], rowHeight)];
        row.title = [NSString stringWithUTF8String:entry.title.empty()
            ? entry.url.c_str() : entry.title.c_str()];
        row.url = [NSString stringWithUTF8String:entry.url.c_str()];
        row.time = [timeFormatter stringFromDate:visitDate];

        __weak SidebarView* weakSelf = self;
        NSString* urlCopy = row.url;
        row.onClick = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            [strongSelf.windowController navigateToURL:urlCopy];
            strongSelf->_activePanel = SidebarPanelTabs;
            [strongSelf updateIconSelection];
            [strongSelf updatePanelVisibility];
        };

        [_historyContainer addSubview:row];
        y += rowHeight;
    }

    _historyContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _historyScrollView.bounds.size.height));
}

#pragma mark - Drawing

- (BOOL)isFlipped { return NO; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[DSColors background] setFill];
    NSRectFill(self.bounds);
}

@end
