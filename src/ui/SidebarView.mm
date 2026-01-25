#import "SidebarView.h"
#import "MainWindowController.h"

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
static const CGFloat kNewTabButtonHeight = 36.0;

#pragma mark - TabRowView

@implementation TabRowView {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    NSButton* _closeButton;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _isHovered = NO;
        _isSelected = NO;
        _isLoading = NO;

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

- (void)closeTab:(id)sender {
    (void)sender;
    [_sidebarView.windowController closeTab:_tabId];
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

    // Draw title
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    style.lineBreakMode = NSLineBreakByTruncatingTail;

    NSDictionary* attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:13],
        NSForegroundColorAttributeName: TextColor(),
        NSParagraphStyleAttributeName: style
    };

    NSRect titleRect = NSMakeRect(12, 10, self.bounds.size.width - 44, 18);
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
    NSView* _workspaceSelector;
    NSScrollView* _tabScrollView;
    FlippedView* _tabContainer;
    NSButton* _newTabButton;
    NSButton* _addWorkspaceBtn;

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

    // Layout from top to bottom: Workspace selector -> Tabs -> New Tab button
    CGFloat contentX = kIconStripWidth;
    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;

    // Workspace selector (at top)
    _workspaceSelector = [[NSView alloc] initWithFrame:NSMakeRect(contentX, self.bounds.size.height - kWorkspaceHeight - 8,
                                                                   contentWidth, kWorkspaceHeight)];
    _workspaceSelector.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [self addSubview:_workspaceSelector];

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

    // New Tab button (at very bottom edge)
    _newTabButton = [[NSButton alloc] initWithFrame:NSMakeRect(contentX + 8, 0, contentWidth - 16, 36)];
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
    [self addSubview:_newTabButton];

    // Tab scroll view (middle area between workspace and new tab button)
    CGFloat tabAreaTop = self.bounds.size.height - kWorkspaceHeight - 16;
    CGFloat tabAreaBottom = kNewTabButtonHeight;
    CGFloat tabAreaHeight = tabAreaTop - tabAreaBottom;

    _tabScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(contentX, tabAreaBottom, contentWidth, tabAreaHeight)];
    _tabScrollView.hasVerticalScroller = YES;
    _tabScrollView.hasHorizontalScroller = NO;
    _tabScrollView.autohidesScrollers = YES;
    _tabScrollView.drawsBackground = NO;
    _tabScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [self addSubview:_tabScrollView];

    _tabContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, contentWidth, tabAreaHeight)];
    _tabScrollView.documentView = _tabContainer;

    [self updateIconSelection];
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

- (BOOL)isFlipped {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [BackgroundColor() setFill];
    NSRectFill(self.bounds);
}

@end
