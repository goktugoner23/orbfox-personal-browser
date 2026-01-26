#import "SidebarView.h"
#import "MainWindowController.h"
#include "history_storage.h"

// Extern function to access global history storage
extern HistoryStorage* GetHistoryStorage();

// Colors
static NSColor* BackgroundColor() {
    return [NSColor colorWithRed:0.11 green:0.11 blue:0.12 alpha:1.0];
}

static NSColor* IconStripColor() {
    return [NSColor colorWithRed:0.08 green:0.08 blue:0.09 alpha:1.0];
}

static NSColor* TabHoverColor() {
    return [NSColor colorWithRed:0.18 green:0.18 blue:0.20 alpha:1.0];
}

static NSColor* TabSelectedColor() {
    return [NSColor colorWithRed:0.22 green:0.22 blue:0.24 alpha:1.0];
}

static NSColor* TextColor() {
    return [NSColor colorWithRed:0.9 green:0.9 blue:0.9 alpha:1.0];
}

static NSColor* SecondaryTextColor() {
    return [NSColor colorWithRed:0.6 green:0.6 blue:0.6 alpha:1.0];
}

static NSColor* AccentColor() {
    return [NSColor colorWithRed:0.0 green:0.48 blue:1.0 alpha:1.0];
}

static const CGFloat kIconStripWidth = 44.0;
static const CGFloat kSidebarWidth = 280.0;
static const CGFloat kWorkspaceHeight = 40.0;
static const CGFloat kTabRowHeight = 36.0;
static const CGFloat kNewTabButtonHeight = 44.0;

#pragma mark - TabRowView

@implementation TabRowView {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    NSButton* _closeButton;
    NSProgressIndicator* _loadingIndicator;
    NSImageView* _faviconView;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _isHovered = NO;
        _isSelected = NO;
        _isLoading = NO;

        // Favicon view
        _faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(10, 10, 16, 16)];
        _faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _faviconView.hidden = YES;  // Hidden until we have a favicon
        [self addSubview:_faviconView];

        // Loading indicator (same position as favicon)
        _loadingIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(10, 10, 16, 16)];
        _loadingIndicator.style = NSProgressIndicatorStyleSpinning;
        _loadingIndicator.controlSize = NSControlSizeSmall;
        _loadingIndicator.displayedWhenStopped = NO;
        [self addSubview:_loadingIndicator];

        // Close button
        _closeButton = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 28, 8, 20, 20)];
        _closeButton.bezelStyle = NSBezelStyleInline;
        _closeButton.bordered = NO;
        _closeButton.title = @"×";
        _closeButton.font = [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
        _closeButton.contentTintColor = SecondaryTextColor();
        _closeButton.target = self;
        _closeButton.action = @selector(closeTab:);
        _closeButton.hidden = YES;
        _closeButton.autoresizingMask = NSViewMinXMargin;
        [self addSubview:_closeButton];
    }
    return self;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
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

    NSMenuItem* closeItem = [[NSMenuItem alloc] initWithTitle:@"Close Tab" action:@selector(closeTab:) keyEquivalent:@""];
    closeItem.target = self;
    [menu addItem:closeItem];

    NSMenuItem* duplicateItem = [[NSMenuItem alloc] initWithTitle:@"Duplicate Tab" action:@selector(duplicateTab:) keyEquivalent:@""];
    duplicateItem.target = self;
    [menu addItem:duplicateItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* reloadItem = [[NSMenuItem alloc] initWithTitle:@"Reload Tab" action:@selector(reloadTab:) keyEquivalent:@""];
    reloadItem.target = self;
    [menu addItem:reloadItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeOthersItem = [[NSMenuItem alloc] initWithTitle:@"Close Other Tabs" action:@selector(closeOtherTabs:) keyEquivalent:@""];
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

    // Collect IDs of tabs to close (all except this one)
    std::vector<int> tabsToClose;
    for (const auto& tab : workspace->tabs) {
        if (tab->id != _tabId) {
            tabsToClose.push_back(tab->id);
        }
    }

    // Close them
    for (int tabId : tabsToClose) {
        [_sidebarView.windowController closeTab:tabId];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    NSColor* bgColor = nil;
    if (_isSelected) {
        bgColor = TabSelectedColor();
    } else if (_isHovered) {
        bgColor = TabHoverColor();
    }

    if (bgColor) {
        [bgColor setFill];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 4, 2)
                                                             xRadius:6
                                                             yRadius:6];
        [path fill];
    }

    // Draw title (offset if loading indicator or favicon is visible)
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    style.lineBreakMode = NSLineBreakByTruncatingTail;

    NSDictionary* attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:13],
        NSForegroundColorAttributeName: TextColor(),
        NSParagraphStyleAttributeName: style
    };

    // Offset title if we have a favicon or loading indicator
    BOOL hasIcon = _isLoading || (_favicon != nil);
    CGFloat titleX = hasIcon ? 32 : 12;
    NSRect titleRect = NSMakeRect(titleX, 10, self.bounds.size.width - titleX - 32, 18);
    [_title drawInRect:titleRect withAttributes:attrs];
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
        // Show favicon if we have one
        _faviconView.hidden = (_favicon == nil);
    }
    [self setNeedsDisplay:YES];
}

- (void)setFavicon:(NSImage*)favicon {
    _favicon = favicon;
    _faviconView.image = favicon;
    // Only show favicon if not loading
    _faviconView.hidden = (_isLoading || favicon == nil);
    [self setNeedsDisplay:YES];
}

@end

// Flipped view for proper top-to-bottom tab ordering
@interface FlippedView : NSView
@end

@implementation FlippedView
- (BOOL)isFlipped {
    return YES;
}
@end

#pragma mark - SidebarView

@implementation SidebarView {
    NSView* _iconStrip;

    // Tabs panel views
    NSView* _tabsPanelContainer;
    NSView* _workspaceSelector;
    NSScrollView* _tabScrollView;
    FlippedView* _tabContainer;
    NSButton* _newTabButton;
    NSButton* _addWorkspaceBtn;

    // History panel views
    NSView* _historyPanelContainer;
    NSScrollView* _historyScrollView;
    FlippedView* _historyContainer;

    NSButton* _tabsIcon;
    NSButton* _favoritesIcon;
    NSButton* _historyIcon;
    NSButton* _downloadsIcon;

    NSMutableArray<TabRowView*>* _tabRows;
}

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
    // Icon strip (leftmost column)
    _iconStrip = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kIconStripWidth, self.bounds.size.height)];
    _iconStrip.wantsLayer = YES;
    _iconStrip.layer.backgroundColor = IconStripColor().CGColor;
    _iconStrip.autoresizingMask = NSViewHeightSizable;
    [self addSubview:_iconStrip];

    // Icon buttons
    CGFloat iconY = self.bounds.size.height - 50;
    CGFloat iconSize = 28;
    CGFloat iconX = (kIconStripWidth - iconSize) / 2;

    _tabsIcon = [self createIconButton:@"square.on.square" frame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _tabsIcon.tag = SidebarPanelTabs;
    [_iconStrip addSubview:_tabsIcon];

    iconY -= 40;
    _favoritesIcon = [self createIconButton:@"star" frame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _favoritesIcon.tag = SidebarPanelFavorites;
    [_iconStrip addSubview:_favoritesIcon];

    iconY -= 40;
    _historyIcon = [self createIconButton:@"clock" frame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _historyIcon.tag = SidebarPanelHistory;
    [_iconStrip addSubview:_historyIcon];

    iconY -= 40;
    _downloadsIcon = [self createIconButton:@"arrow.down.circle" frame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _downloadsIcon.tag = SidebarPanelDownloads;
    [_iconStrip addSubview:_downloadsIcon];

    // Content area dimensions
    CGFloat contentX = kIconStripWidth;
    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;
    CGFloat contentHeight = self.bounds.size.height;

    // ========================================
    // TABS PANEL
    // ========================================
    _tabsPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(contentX, 0, contentWidth, contentHeight)];
    _tabsPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [self addSubview:_tabsPanelContainer];

    // Workspace selector (at top)
    _workspaceSelector = [[NSView alloc] initWithFrame:NSMakeRect(0, contentHeight - kWorkspaceHeight - 8,
                                                                   contentWidth, kWorkspaceHeight)];
    _workspaceSelector.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [_tabsPanelContainer addSubview:_workspaceSelector];

    // Workspace button
    NSButton* workspaceBtn = [[NSButton alloc] initWithFrame:NSMakeRect(8, 4, 100, 32)];
    workspaceBtn.bezelStyle = NSBezelStyleInline;
    workspaceBtn.bordered = NO;
    workspaceBtn.title = @"● Personal";
    workspaceBtn.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    workspaceBtn.contentTintColor = TextColor();
    workspaceBtn.wantsLayer = YES;
    workspaceBtn.layer.backgroundColor = TabSelectedColor().CGColor;
    workspaceBtn.layer.cornerRadius = 6;
    [_workspaceSelector addSubview:workspaceBtn];

    // Add workspace button
    _addWorkspaceBtn = [[NSButton alloc] initWithFrame:NSMakeRect(contentWidth - 36, 8, 24, 24)];
    _addWorkspaceBtn.bezelStyle = NSBezelStyleInline;
    _addWorkspaceBtn.bordered = NO;
    _addWorkspaceBtn.image = [NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:@"Add workspace"];
    _addWorkspaceBtn.contentTintColor = SecondaryTextColor();
    _addWorkspaceBtn.autoresizingMask = NSViewMinXMargin;
    _addWorkspaceBtn.target = self;
    _addWorkspaceBtn.action = @selector(addWorkspaceClicked:);
    [_workspaceSelector addSubview:_addWorkspaceBtn];

    // New Tab button (at bottom)
    _newTabButton = [[NSButton alloc] initWithFrame:NSMakeRect(8, 5, contentWidth - 16, 34)];
    _newTabButton.bezelStyle = NSBezelStyleInline;
    _newTabButton.bordered = NO;
    _newTabButton.title = @"";
    _newTabButton.image = [NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:@"New Tab"];
    _newTabButton.imagePosition = NSImageLeading;
    _newTabButton.contentTintColor = SecondaryTextColor();
    _newTabButton.target = self;
    _newTabButton.action = @selector(newTabClicked:);
    _newTabButton.autoresizingMask = NSViewMaxYMargin | NSViewWidthSizable;

    NSMutableAttributedString* newTabTitle = [[NSMutableAttributedString alloc] initWithString:@"  New Tab"];
    [newTabTitle addAttribute:NSForegroundColorAttributeName value:SecondaryTextColor() range:NSMakeRange(0, newTabTitle.length)];
    [newTabTitle addAttribute:NSFontAttributeName value:[NSFont systemFontOfSize:13] range:NSMakeRange(0, newTabTitle.length)];
    _newTabButton.attributedTitle = newTabTitle;
    [_tabsPanelContainer addSubview:_newTabButton];

    // Tab scroll view
    CGFloat tabAreaTop = contentHeight - kWorkspaceHeight - 16;
    CGFloat tabAreaBottom = kNewTabButtonHeight;
    CGFloat tabAreaHeight = tabAreaTop - tabAreaBottom;

    _tabScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, tabAreaBottom, contentWidth, tabAreaHeight)];
    _tabScrollView.hasVerticalScroller = YES;
    _tabScrollView.hasHorizontalScroller = NO;
    _tabScrollView.autohidesScrollers = YES;
    _tabScrollView.drawsBackground = NO;
    _tabScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_tabsPanelContainer addSubview:_tabScrollView];

    _tabContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, contentWidth, tabAreaHeight)];
    _tabScrollView.documentView = _tabContainer;

    // ========================================
    // HISTORY PANEL
    // ========================================
    _historyPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(contentX, 0, contentWidth, contentHeight)];
    _historyPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _historyPanelContainer.hidden = YES;
    [self addSubview:_historyPanelContainer];

    // History title
    NSTextField* historyTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(12, contentHeight - 40, contentWidth - 24, 24)];
    historyTitle.stringValue = @"History";
    historyTitle.font = [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];
    historyTitle.textColor = TextColor();
    historyTitle.bezeled = NO;
    historyTitle.drawsBackground = NO;
    historyTitle.editable = NO;
    historyTitle.selectable = NO;
    historyTitle.autoresizingMask = NSViewMinYMargin;
    [_historyPanelContainer addSubview:historyTitle];

    // History scroll view
    _historyScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, contentWidth, contentHeight - 50)];
    _historyScrollView.hasVerticalScroller = YES;
    _historyScrollView.hasHorizontalScroller = NO;
    _historyScrollView.autohidesScrollers = YES;
    _historyScrollView.drawsBackground = NO;
    _historyScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_historyPanelContainer addSubview:_historyScrollView];

    _historyContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, contentWidth, contentHeight - 50)];
    _historyScrollView.documentView = _historyContainer;

    [self updateIconSelection];
    [self updatePanelVisibility];
}

- (NSButton*)createIconButton:(NSString*)symbolName frame:(NSRect)frame {
    NSButton* btn = [[NSButton alloc] initWithFrame:frame];
    btn.bezelStyle = NSBezelStyleInline;
    btn.bordered = NO;
    btn.image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:symbolName];
    btn.contentTintColor = SecondaryTextColor();
    btn.target = self;
    btn.action = @selector(iconClicked:);
    btn.autoresizingMask = NSViewMinYMargin;
    return btn;
}

- (void)iconClicked:(NSButton*)sender {
    _activePanel = (SidebarPanel)sender.tag;
    [self updateIconSelection];
    [self updatePanelVisibility];
}

- (void)updatePanelVisibility {
    // Hide all panels
    _tabsPanelContainer.hidden = YES;
    _historyPanelContainer.hidden = YES;

    // Show active panel
    switch (_activePanel) {
        case SidebarPanelTabs:
            _tabsPanelContainer.hidden = NO;
            break;
        case SidebarPanelHistory:
            _historyPanelContainer.hidden = NO;
            [self reloadHistory];
            break;
        default:
            // Favorites and Downloads - show placeholder (TODO)
            break;
    }
}

- (void)updateIconSelection {
    _tabsIcon.contentTintColor = (_activePanel == SidebarPanelTabs) ? AccentColor() : SecondaryTextColor();
    _favoritesIcon.contentTintColor = (_activePanel == SidebarPanelFavorites) ? AccentColor() : SecondaryTextColor();
    _historyIcon.contentTintColor = (_activePanel == SidebarPanelHistory) ? AccentColor() : SecondaryTextColor();
    _downloadsIcon.contentTintColor = (_activePanel == SidebarPanelDownloads) ? AccentColor() : SecondaryTextColor();
}

- (void)newTabClicked:(id)sender {
    (void)sender;
    [_windowController createNewTab:@""];
}

- (void)addWorkspaceClicked:(id)sender {
    (void)sender;
    // TODO: Implement workspace creation UI
    // For now, just create a new workspace with default name
    if (_windowController && _windowController.tabManager) {
        _windowController.tabManager->CreateWorkspace("New Space");
        // TODO: Update workspace selector UI
    }
}

- (void)reloadTabs {
    // Clear existing tab rows
    for (TabRowView* row in _tabRows) {
        [row removeFromSuperview];
    }
    [_tabRows removeAllObjects];

    if (!_windowController || !_windowController.tabManager) {
        return;
    }

    Workspace* workspace = _windowController.tabManager->GetActiveWorkspace();
    if (!workspace) {
        return;
    }

    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;
    CGFloat totalHeight = workspace->tabs.size() * kTabRowHeight;
    CGFloat minHeight = _tabScrollView.bounds.size.height;

    // Ensure container is at least as tall as scroll view
    _tabContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(totalHeight, minHeight));

    // Position tabs from top to bottom (y=0 is at top in flipped view)
    CGFloat y = 0;
    int activeTabId = workspace->GetActiveTab() ? workspace->GetActiveTab()->id : -1;

    for (const auto& tab : workspace->tabs) {
        TabRowView* row = [[TabRowView alloc] initWithFrame:NSMakeRect(0, y, contentWidth, kTabRowHeight)];
        row.tabId = tab->id;
        row.title = [NSString stringWithUTF8String:tab->title.c_str()];
        row.isSelected = (tab->id == activeTabId);
        row.isLoading = tab->is_loading;
        row.sidebarView = self;

        // Set favicon if available
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
        y += kTabRowHeight;
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

- (void)reloadHistory {
    // Clear existing history rows
    for (NSView* subview in _historyContainer.subviews.copy) {
        [subview removeFromSuperview];
    }

    HistoryStorage* history = GetHistoryStorage();
    if (!history) return;

    std::vector<HistoryEntry> entries = history->GetRecentHistory(100);
    CGFloat contentWidth = _historyContainer.bounds.size.width;
    CGFloat y = 0;
    CGFloat rowHeight = 48;

    // Group by date
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

        // Add date header if different from last
        if (![dateString isEqualToString:lastDateString]) {
            NSTextField* dateHeader = [[NSTextField alloc] initWithFrame:NSMakeRect(12, y, contentWidth - 24, 24)];
            dateHeader.stringValue = dateString;
            dateHeader.font = [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
            dateHeader.textColor = SecondaryTextColor();
            dateHeader.bezeled = NO;
            dateHeader.drawsBackground = NO;
            dateHeader.editable = NO;
            dateHeader.selectable = NO;
            [_historyContainer addSubview:dateHeader];
            y += 28;
            lastDateString = dateString;
        }

        // History row
        NSView* row = [[NSView alloc] initWithFrame:NSMakeRect(0, y, contentWidth, rowHeight)];

        // Title
        NSTextField* titleField = [[NSTextField alloc] initWithFrame:NSMakeRect(12, 24, contentWidth - 24, 18)];
        titleField.stringValue = [NSString stringWithUTF8String:entry.title.empty() ? entry.url.c_str() : entry.title.c_str()];
        titleField.font = [NSFont systemFontOfSize:13];
        titleField.textColor = TextColor();
        titleField.bezeled = NO;
        titleField.drawsBackground = NO;
        titleField.editable = NO;
        titleField.selectable = NO;
        titleField.lineBreakMode = NSLineBreakByTruncatingTail;
        [row addSubview:titleField];

        // URL and time
        NSString* urlStr = [NSString stringWithUTF8String:entry.url.c_str()];
        NSString* timeStr = [timeFormatter stringFromDate:visitDate];
        NSTextField* subtitleField = [[NSTextField alloc] initWithFrame:NSMakeRect(12, 6, contentWidth - 70, 16)];
        subtitleField.stringValue = urlStr;
        subtitleField.font = [NSFont systemFontOfSize:11];
        subtitleField.textColor = SecondaryTextColor();
        subtitleField.bezeled = NO;
        subtitleField.drawsBackground = NO;
        subtitleField.editable = NO;
        subtitleField.selectable = NO;
        subtitleField.lineBreakMode = NSLineBreakByTruncatingTail;
        [row addSubview:subtitleField];

        NSTextField* timeField = [[NSTextField alloc] initWithFrame:NSMakeRect(contentWidth - 55, 6, 50, 16)];
        timeField.stringValue = timeStr;
        timeField.font = [NSFont systemFontOfSize:11];
        timeField.textColor = SecondaryTextColor();
        timeField.bezeled = NO;
        timeField.drawsBackground = NO;
        timeField.editable = NO;
        timeField.selectable = NO;
        timeField.alignment = NSTextAlignmentRight;
        [row addSubview:timeField];

        // Store URL for click handling
        row.identifier = urlStr;

        // Click gesture
        NSClickGestureRecognizer* click = [[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(historyRowClicked:)];
        [row addGestureRecognizer:click];

        [_historyContainer addSubview:row];
        y += rowHeight;
    }

    // Resize container
    _historyContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _historyScrollView.bounds.size.height));
}

- (void)historyRowClicked:(NSClickGestureRecognizer*)gesture {
    NSView* row = gesture.view;
    if (row.identifier) {
        [_windowController navigateToURL:row.identifier];
        // Switch back to tabs panel
        _activePanel = SidebarPanelTabs;
        [self updateIconSelection];
        [self updatePanelVisibility];
    }
}

- (BOOL)isFlipped {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [BackgroundColor() setFill];
    NSRectFill(self.bounds);
}

@end
