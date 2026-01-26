#import "SidebarView.h"
#import "MainWindowController.h"
#import "Components.h"
#include "history_storage.h"

// Extern function to access global history storage
extern HistoryStorage* GetHistoryStorage();

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
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    _closeButton.hidden = YES;
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

@end

// ============================================================================
// SIDEBAR VIEW
// ============================================================================

@implementation SidebarView {
    NSView* _iconStrip;

    // Tabs panel
    NSView* _tabsPanelContainer;
    NSView* _workspaceSelector;
    NSScrollView* _tabScrollView;
    FlippedView* _tabContainer;
    DSButton* _newTabButton;
    DSIconButton* _addWorkspaceBtn;

    // History panel
    NSView* _historyPanelContainer;
    NSScrollView* _historyScrollView;
    FlippedView* _historyContainer;

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
    _favoritesIcon = [self createIconButton:@"star" y:iconY tooltip:@"Bookmarks"];
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
    [self setupHistoryPanel:contentX width:contentWidth height:contentHeight];

    [self updateIconSelection];
    [self updatePanelVisibility];
}

- (DSIconButton*)createIconButton:(NSString*)symbolName y:(CGFloat)y tooltip:(NSString*)tooltip {
    CGFloat iconSize = [DSLayout iconSizeLarge];
    CGFloat iconX = (kIconStripWidth - iconSize) / 2;

    DSIconButton* btn = [DSIconButton buttonWithIcon:symbolName tooltip:tooltip];
    btn.frame = NSMakeRect(iconX, y, iconSize, iconSize);
    btn.target = self;
    btn.action = @selector(iconClicked:);
    btn.autoresizingMask = NSViewMinYMargin;
    return btn;
}

- (void)setupTabsPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _tabsPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _tabsPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [self addSubview:_tabsPanelContainer];

    // Workspace selector
    _workspaceSelector = [[NSView alloc] initWithFrame:NSMakeRect(
        0, height - kWorkspaceHeight - [DSSpacing sm], width, kWorkspaceHeight)];
    _workspaceSelector.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [_tabsPanelContainer addSubview:_workspaceSelector];

    // Workspace button
    DSButton* workspaceBtn = [DSButton buttonWithTitle:@"Personal" variant:DSButtonVariantSubtle];
    workspaceBtn.frame = NSMakeRect([DSSpacing sm], [DSSpacing xs], 100, 32);
    [_workspaceSelector addSubview:workspaceBtn];

    // Add workspace button
    _addWorkspaceBtn = [DSIconButton buttonWithIcon:@"plus" tooltip:@"Add Workspace"];
    _addWorkspaceBtn.frame = NSMakeRect(width - 36, [DSSpacing sm], 24, 24);
    _addWorkspaceBtn.autoresizingMask = NSViewMinXMargin;
    _addWorkspaceBtn.target = self;
    _addWorkspaceBtn.action = @selector(addWorkspaceClicked:);
    [_workspaceSelector addSubview:_addWorkspaceBtn];

    // New Tab button
    _newTabButton = [DSButton buttonWithTitle:@"New Tab" icon:@"plus" variant:DSButtonVariantGhost];
    _newTabButton.frame = NSMakeRect([DSSpacing sm], 5, width - [DSSpacing lg], 34);
    _newTabButton.imagePosition = NSImageTrailing;
    _newTabButton.autoresizingMask = NSViewMaxYMargin | NSViewWidthSizable;
    _newTabButton.target = self;
    _newTabButton.action = @selector(newTabClicked:);
    [_tabsPanelContainer addSubview:_newTabButton];

    // Tab scroll view
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

- (void)setupHistoryPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _historyPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _historyPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
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
    if (_windowController && _windowController.tabManager) {
        _windowController.tabManager->CreateWorkspace("New Space");
    }
}

#pragma mark - UI Updates

- (void)updatePanelVisibility {
    _tabsPanelContainer.hidden = YES;
    _historyPanelContainer.hidden = YES;

    switch (_activePanel) {
        case SidebarPanelTabs:
            _tabsPanelContainer.hidden = NO;
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
    _tabsIcon.contentTintColor = (_activePanel == SidebarPanelTabs)
        ? [DSColors accent] : [DSColors textSecondary];
    _favoritesIcon.contentTintColor = (_activePanel == SidebarPanelFavorites)
        ? [DSColors accent] : [DSColors textSecondary];
    _historyIcon.contentTintColor = (_activePanel == SidebarPanelHistory)
        ? [DSColors accent] : [DSColors textSecondary];
    _downloadsIcon.contentTintColor = (_activePanel == SidebarPanelDownloads)
        ? [DSColors accent] : [DSColors textSecondary];
}

#pragma mark - Tabs

- (void)reloadTabs {
    for (TabRowView* row in _tabRows) {
        [row removeFromSuperview];
    }
    [_tabRows removeAllObjects];

    if (!_windowController || !_windowController.tabManager) return;

    Workspace* workspace = _windowController.tabManager->GetActiveWorkspace();
    if (!workspace) return;

    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;
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
