#import "SidebarView.h"
#import "MainWindowController.h"
#import "Components.h"
#import "SidebarHelperViews.h"
#import "CircularProgressView.h"
#import "DownloadRowView.h"
#import "TabRowView.h"
#import "BookmarkDropContainerView.h"
#import "AddBookmarkPopoverController.h"
#import <objc/runtime.h>
#include "history_storage.h"
#include "bookmark_storage.h"
#include "download_manager.h"

// Layout constants
static const CGFloat kIconStripWidth = 44.0;
static const CGFloat kSidebarWidth = 280.0;
static const CGFloat kWorkspaceHeight = 48.0;  // Total height of workspace row (taller to avoid clipping tabs)
static const CGFloat kNewTabButtonHeight = 44.0;

// ============================================================================
// FAVICON & TITLE CACHE
// Persistent cache for favicons and page titles by domain
// Stored in ~/Library/Application Support/OrbFox/cache/
// ============================================================================

static NSMutableDictionary<NSString*, NSImage*>* sFaviconCache = nil;
static NSMutableDictionary<NSString*, NSString*>* sTitleCache = nil;
static BOOL sCacheInitialized = NO;

static NSString* GetCacheDirectory() {
    NSString* home = NSHomeDirectory();
    NSString* cacheDir = [home stringByAppendingPathComponent:@"Library/Application Support/OrbFox/cache"];

    // Create directory if it doesn't exist
    NSFileManager* fm = [NSFileManager defaultManager];
    if (![fm fileExistsAtPath:cacheDir]) {
        [fm createDirectoryAtPath:cacheDir withIntermediateDirectories:YES attributes:nil error:nil];
    }
    return cacheDir;
}

static NSString* GetFaviconPath(NSString* domain) {
    return [[GetCacheDirectory() stringByAppendingPathComponent:domain] stringByAppendingPathExtension:@"png"];
}

static NSString* GetTitleCachePath() {
    return [GetCacheDirectory() stringByAppendingPathComponent:@"titles.plist"];
}

static NSString* GetDomainFromURL(NSString* urlString) {
    if (!urlString || urlString.length == 0) return nil;
    NSURL* url = [NSURL URLWithString:urlString];
    NSString* host = url.host;
    if (!host) return nil;
    // Remove www. prefix for consistent matching
    if ([host hasPrefix:@"www."]) {
        host = [host substringFromIndex:4];
    }
    return host;
}

static void LoadCachedFaviconsAndTitles() {
    if (sCacheInitialized) return;
    sCacheInitialized = YES;

    sFaviconCache = [NSMutableDictionary dictionary];
    sTitleCache = [NSMutableDictionary dictionary];

    NSString* cacheDir = GetCacheDirectory();
    NSFileManager* fm = [NSFileManager defaultManager];

    // Load favicons
    NSArray* files = [fm contentsOfDirectoryAtPath:cacheDir error:nil];
    for (NSString* file in files) {
        if ([file.pathExtension isEqualToString:@"png"]) {
            NSString* domain = [file stringByDeletingPathExtension];
            NSString* path = [cacheDir stringByAppendingPathComponent:file];
            NSImage* favicon = [[NSImage alloc] initWithContentsOfFile:path];
            if (favicon) {
                sFaviconCache[domain] = favicon;
            }
        }
    }

    // Load titles
    NSString* titlesPath = GetTitleCachePath();
    if ([fm fileExistsAtPath:titlesPath]) {
        NSDictionary* titles = [NSDictionary dictionaryWithContentsOfFile:titlesPath];
        if (titles) {
            [sTitleCache addEntriesFromDictionary:titles];
        }
    }
}

NSImage* GetCachedFavicon(NSString* urlString) {
    LoadCachedFaviconsAndTitles();
    NSString* domain = GetDomainFromURL(urlString);
    if (!domain) return nil;
    return sFaviconCache[domain];
}

static void CacheFavicon(NSString* urlString, NSImage* favicon) {
    if (!favicon || !urlString) return;
    LoadCachedFaviconsAndTitles();

    NSString* domain = GetDomainFromURL(urlString);
    if (!domain) return;

    // Always update the cache - allows correcting stale/wrong favicons
    sFaviconCache[domain] = favicon;

    // Save to disk asynchronously
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
        NSString* path = GetFaviconPath(domain);
        NSBitmapImageRep* rep = nil;

        // Get bitmap representation
        for (NSImageRep* imageRep in favicon.representations) {
            if ([imageRep isKindOfClass:[NSBitmapImageRep class]]) {
                rep = (NSBitmapImageRep*)imageRep;
                break;
            }
        }

        if (!rep) {
            // Create bitmap from image
            NSSize size = favicon.size;
            if (size.width == 0 || size.height == 0) {
                size = NSMakeSize(32, 32);
            }
            rep = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:NULL
                              pixelsWide:(NSInteger)size.width
                              pixelsHigh:(NSInteger)size.height
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSCalibratedRGBColorSpace
                             bytesPerRow:0
                            bitsPerPixel:0];

            NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:ctx];
            [favicon drawInRect:NSMakeRect(0, 0, size.width, size.height)];
            [NSGraphicsContext restoreGraphicsState];
        }

        NSData* pngData = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        [pngData writeToFile:path atomically:YES];
    });
}

static NSString* GetCachedTitle(NSString* urlString) {
    LoadCachedFaviconsAndTitles();
    NSString* domain = GetDomainFromURL(urlString);
    if (!domain) return nil;
    return sTitleCache[domain];
}

static void CacheTitle(NSString* urlString, NSString* title) {
    if (!title || title.length == 0 || !urlString) return;
    LoadCachedFaviconsAndTitles();

    NSString* domain = GetDomainFromURL(urlString);
    if (!domain) return;

    // Check if already cached with same title
    if ([sTitleCache[domain] isEqualToString:title]) return;

    sTitleCache[domain] = title;

    // Save to disk asynchronously
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
        NSString* path = GetTitleCachePath();
        [sTitleCache writeToFile:path atomically:YES];
    });
}

// ============================================================================
// SIDEBAR VIEW
// ============================================================================

// Helper: Convert hex color string (e.g. "#007AFF") to NSColor
static NSColor* NSColorFromHex(const std::string& hex) {
    if (hex.length() < 7 || hex[0] != '#') {
        return [NSColor systemBlueColor];
    }
    unsigned int r = 0, g = 0, b = 0;
    sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    return [NSColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0];
}

@implementation SidebarView {
    NSView* _iconStrip;

    // Tabs panel
    NSView* _tabsPanelContainer;
    NSView* _workspaceSelector;
    NSScrollView* _workspaceScrollView;  // Scrollable workspace tabs
    WorkspaceDropContainerView* _workspaceTabsContainer;
    NSScrollView* _tabScrollView;
    TabDropContainerView* _tabContainer;
    DSButton* _newTabButton;
    DSIconButton* _addWorkspaceBtn;
    NSMutableArray<NSView*>* _workspaceTabs;  // Array of workspace tab containers

    // History panel
    NSView* _historyPanelContainer;
    NSTextField* _historySearchField;
    NSScrollView* _historyScrollView;
    FlippedView* _historyContainer;
    NSString* _historySearchQuery;

    // Bookmarks panel
    NSView* _bookmarksPanelContainer;
    NSScrollView* _bookmarksScrollView;
    BookmarkDropContainerView* _bookmarksContainer;
    NSMutableSet<NSString*>* _collapsedFolders;
    int64_t _selectedBookmarkId;
    AddBookmarkPopoverController* _addBookmarkPopover;
    DSIconButton* _addBookmarkButton;

    // Downloads panel
    NSView* _downloadsPanelContainer;
    NSScrollView* _downloadsScrollView;
    FlippedView* _downloadsContainer;
    DownloadRowView* _selectedDownloadRow;

    // Icon buttons
    DSIconButton* _tabsIcon;
    DSIconButton* _favoritesIcon;
    DSIconButton* _historyIcon;
    DSIconButton* _downloadsIcon;
    DSIconButton* _settingsIcon;
    CircularProgressView* _downloadProgressRing;  // Progress ring around downloads icon

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
        _collapsedFolders = [NSMutableSet set];
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

    // Rounded rect progress ring around downloads icon (same size as icon)
    _downloadProgressRing = [[CircularProgressView alloc] initWithFrame:NSMakeRect(
        iconX, iconY, iconSize, iconSize)];
    _downloadProgressRing.hidden = YES;
    _downloadProgressRing.autoresizingMask = NSViewMinYMargin;
    [_iconStrip addSubview:_downloadProgressRing positioned:NSWindowAbove relativeTo:_downloadsIcon];

    // Settings icon at the bottom of the icon strip
    _settingsIcon = [self createIconButton:@"gearshape" y:20 tooltip:@"Settings"];
    _settingsIcon.tag = 999;  // Special tag for settings (not a panel)
    _settingsIcon.showsHoverBackground = YES;
    _settingsIcon.autoresizingMask = NSViewMaxYMargin;  // Stay at bottom
    [_iconStrip addSubview:_settingsIcon];

    // Content area
    CGFloat contentX = kIconStripWidth;
    CGFloat contentWidth = kSidebarWidth - kIconStripWidth;
    CGFloat contentHeight = self.bounds.size.height;

    [self setupTabsPanel:contentX width:contentWidth height:contentHeight];
    [self setupBookmarksPanel:contentX width:contentWidth height:contentHeight];
    [self setupHistoryPanel:contentX width:contentWidth height:contentHeight];
    [self setupDownloadsPanel:contentX width:contentWidth height:contentHeight];

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

    // Workspace selector row at top (flush with top edge)
    _workspaceSelector = [[NSView alloc] initWithFrame:NSMakeRect(
        0, height - kWorkspaceHeight, width, kWorkspaceHeight)];
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
    _workspaceScrollView = [[HorizontalScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, scrollWidth, kWorkspaceHeight)];
    _workspaceScrollView.hasHorizontalScroller = YES;
    _workspaceScrollView.hasVerticalScroller = NO;
    _workspaceScrollView.horizontalScroller.alphaValue = 0;
    _workspaceScrollView.drawsBackground = NO;
    _workspaceScrollView.autoresizingMask = NSViewWidthSizable;
    _workspaceScrollView.horizontalScrollElasticity = NSScrollElasticityAllowed;
    [_workspaceSelector addSubview:_workspaceScrollView];

    // Container for workspace tab buttons (with drop support for reordering)
    _workspaceTabsContainer = [[WorkspaceDropContainerView alloc] initWithFrame:NSMakeRect(0, 0, scrollWidth, kWorkspaceHeight)];
    _workspaceTabsContainer.sidebarView = self;
    _workspaceScrollView.documentView = _workspaceTabsContainer;

    // Ensure scroll position starts at origin
    [_workspaceScrollView.contentView scrollToPoint:NSZeroPoint];
    [_workspaceScrollView reflectScrolledClipView:_workspaceScrollView.contentView];

    // New Tab button at bottom - same size as tab rows (4pt inset on sides like TabRowView)
    CGFloat tabInset = [DSSpacing xs];
    _newTabButton = [DSButton buttonWithTitle:@"New Tab" variant:DSButtonVariantGhost];
    _newTabButton.alignment = NSTextAlignmentCenter;
    _newTabButton.frame = NSMakeRect(tabInset, padding, width - tabInset * 2, 34);
    _newTabButton.autoresizingMask = NSViewMaxYMargin;
    _newTabButton.target = self;
    _newTabButton.action = @selector(newTabClicked:);
    [_tabsPanelContainer addSubview:_newTabButton];

    // Tab scroll view - between workspace selector and new tab button
    CGFloat tabAreaTop = height - kWorkspaceHeight;
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

    _tabContainer = [[TabDropContainerView alloc] initWithFrame:NSMakeRect(0, 0, width, tabAreaHeight)];
    _tabContainer.sidebarView = self;
    _tabScrollView.documentView = _tabContainer;
}

// Called when sidebar width changes (from resize)
- (void)updateLayoutForWidth:(CGFloat)newWidth {
    CGFloat contentWidth = newWidth - kIconStripWidth;

    // Update tab container width
    NSRect containerFrame = _tabContainer.frame;
    containerFrame.size.width = contentWidth;
    _tabContainer.frame = containerFrame;

    // Update New Tab button width (maintain margins like tabs)
    CGFloat tabInset = [DSSpacing xs];
    NSRect buttonFrame = _newTabButton.frame;
    buttonFrame.size.width = contentWidth - tabInset * 2;
    _newTabButton.frame = buttonFrame;

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
        [DSSpacing md], height - 40, width - [DSSpacing xl] - 64, 24)];
    bookmarksTitle.stringValue = @"Bookmarks";
    bookmarksTitle.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    bookmarksTitle.textColor = [DSColors textPrimary];
    bookmarksTitle.bezeled = NO;
    bookmarksTitle.drawsBackground = NO;
    bookmarksTitle.editable = NO;
    bookmarksTitle.selectable = NO;
    bookmarksTitle.autoresizingMask = NSViewMinYMargin;
    [_bookmarksPanelContainer addSubview:bookmarksTitle];

    // New folder button
    DSIconButton* newFolderBtn = [DSIconButton buttonWithIcon:@"folder.badge.plus" tooltip:@"New folder"];
    newFolderBtn.frame = NSMakeRect(width - 68, height - 40, 28, 28);
    newFolderBtn.autoresizingMask = NSViewMinYMargin | NSViewMinXMargin;
    newFolderBtn.target = self;
    newFolderBtn.action = @selector(newFolderClicked:);
    [_bookmarksPanelContainer addSubview:newFolderBtn];

    // Add bookmark button (plus icon - shows menu on click)
    _addBookmarkButton = [DSIconButton buttonWithIcon:@"plus" tooltip:@"Add bookmark or folder"];
    _addBookmarkButton.frame = NSMakeRect(width - 36, height - 40, 28, 28);
    _addBookmarkButton.autoresizingMask = NSViewMinYMargin | NSViewMinXMargin;
    _addBookmarkButton.target = self;
    _addBookmarkButton.action = @selector(addBookmarkClicked:);
    [_bookmarksPanelContainer addSubview:_addBookmarkButton];

    // Bookmarks scroll view
    _bookmarksScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, width, height - 50)];
    _bookmarksScrollView.hasVerticalScroller = YES;
    _bookmarksScrollView.hasHorizontalScroller = NO;
    _bookmarksScrollView.autohidesScrollers = YES;
    _bookmarksScrollView.drawsBackground = NO;
    _bookmarksScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_bookmarksPanelContainer addSubview:_bookmarksScrollView];

    _bookmarksContainer = [[BookmarkDropContainerView alloc] initWithFrame:NSMakeRect(0, 0, width, height - 50)];
    _bookmarksContainer.sidebarView = self;
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
        [DSSpacing md], height - 40, width - [DSSpacing xl] - 50, 24)];
    historyTitle.stringValue = @"History";
    historyTitle.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    historyTitle.textColor = [DSColors textPrimary];
    historyTitle.bezeled = NO;
    historyTitle.drawsBackground = NO;
    historyTitle.editable = NO;
    historyTitle.selectable = NO;
    historyTitle.autoresizingMask = NSViewMinYMargin;
    [_historyPanelContainer addSubview:historyTitle];

    // Clear history button
    DSButton* clearButton = [DSButton buttonWithTitle:@"Clear" variant:DSButtonVariantGhost];
    clearButton.frame = NSMakeRect(width - 55, height - 42, 50, 24);
    clearButton.autoresizingMask = NSViewMinYMargin | NSViewMinXMargin;
    clearButton.target = self;
    clearButton.action = @selector(clearHistoryClicked:);
    [_historyPanelContainer addSubview:clearButton];

    // History search field
    CGFloat searchFieldY = height - 76;
    _historySearchField = [[NSTextField alloc] initWithFrame:NSMakeRect(
        [DSSpacing sm], searchFieldY, width - [DSSpacing md], 28)];
    _historySearchField.placeholderString = @"Search history...";
    _historySearchField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _historySearchField.bezelStyle = NSTextFieldRoundedBezel;
    _historySearchField.delegate = self;
    _historySearchField.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [_historyPanelContainer addSubview:_historySearchField];

    // History scroll view (adjusted for search field)
    CGFloat scrollViewHeight = height - 86;
    _historyScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, width, scrollViewHeight)];
    _historyScrollView.hasVerticalScroller = YES;
    _historyScrollView.hasHorizontalScroller = NO;
    _historyScrollView.autohidesScrollers = YES;
    _historyScrollView.drawsBackground = NO;
    _historyScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_historyPanelContainer addSubview:_historyScrollView];

    _historyContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, scrollViewHeight)];
    _historyContainer.autoresizingMask = NSViewWidthSizable;
    _historyScrollView.documentView = _historyContainer;
}

- (void)setupDownloadsPanel:(CGFloat)x width:(CGFloat)width height:(CGFloat)height {
    _downloadsPanelContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, 0, width, height)];
    _downloadsPanelContainer.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _downloadsPanelContainer.wantsLayer = YES;
    _downloadsPanelContainer.layer.masksToBounds = YES;
    _downloadsPanelContainer.hidden = YES;
    [self addSubview:_downloadsPanelContainer];

    // Downloads title
    NSTextField* downloadsTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(
        [DSSpacing md], height - 40, width - [DSSpacing xl] - 32, 24)];
    downloadsTitle.stringValue = @"Downloads";
    downloadsTitle.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    downloadsTitle.textColor = [DSColors textPrimary];
    downloadsTitle.bezeled = NO;
    downloadsTitle.drawsBackground = NO;
    downloadsTitle.editable = NO;
    downloadsTitle.selectable = NO;
    downloadsTitle.autoresizingMask = NSViewMinYMargin;
    [_downloadsPanelContainer addSubview:downloadsTitle];

    // Clear completed button
    DSIconButton* clearBtn = [DSIconButton buttonWithIcon:@"trash" tooltip:@"Clear completed"];
    clearBtn.frame = NSMakeRect(width - 36, height - 40, 28, 28);
    clearBtn.autoresizingMask = NSViewMinYMargin | NSViewMinXMargin;
    clearBtn.target = self;
    clearBtn.action = @selector(clearCompletedDownloads:);
    [_downloadsPanelContainer addSubview:clearBtn];

    // Downloads scroll view
    _downloadsScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(
        0, 0, width, height - 50)];
    _downloadsScrollView.hasVerticalScroller = YES;
    _downloadsScrollView.hasHorizontalScroller = NO;
    _downloadsScrollView.autohidesScrollers = YES;
    _downloadsScrollView.drawsBackground = NO;
    _downloadsScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    [_downloadsPanelContainer addSubview:_downloadsScrollView];

    _downloadsContainer = [[FlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, height - 50)];
    _downloadsContainer.autoresizingMask = NSViewWidthSizable;
    _downloadsScrollView.documentView = _downloadsContainer;

    // Load download history from disk
    DownloadManager::GetInstance().LoadFromDisk();

    // Set up download manager callback for real-time updates
    __weak SidebarView* weakSelf = self;
    DownloadManager::GetInstance().SetUpdateCallback([weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;

            // Update downloads panel if visible
            if (strongSelf->_activePanel == SidebarPanelDownloads) {
                [strongSelf reloadDownloads];
            }

            // Update progress indicator on downloads icon
            [strongSelf updateDownloadProgressIndicator];
        });
    });
}

- (void)updateDownloadProgressIndicator {
    float progress = DownloadManager::GetInstance().GetOverallProgress();

    if (progress < 0) {
        // No active downloads OR unknown size - hide progress ring
        _downloadProgressRing.hidden = YES;
    } else {
        _downloadProgressRing.hidden = NO;
        _downloadProgressRing.progress = progress;
    }
}

#pragma mark - Actions

- (void)iconClicked:(NSButton*)sender {
    // Special handling for settings icon (tag 999)
    if (sender.tag == 999) {
        // Open settings in a new tab
        if (_windowController) {
            [_windowController openSettingsInNewTab];
        }
        return;
    }

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

// NSGestureRecognizerDelegate - don't intercept clicks on buttons
- (BOOL)gestureRecognizer:(NSGestureRecognizer*)gestureRecognizer shouldReceiveTouch:(NSTouch*)touch {
    return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(NSGestureRecognizer*)gestureRecognizer {
    // Check if click is on a button - if so, don't begin the gesture
    NSPoint locationInView = [gestureRecognizer locationInView:gestureRecognizer.view];
    NSView* hitView = [gestureRecognizer.view hitTest:[gestureRecognizer.view convertPoint:locationInView toView:gestureRecognizer.view.superview]];

    // If the hit view is a button or inside a button, don't handle the gesture
    while (hitView && hitView != gestureRecognizer.view) {
        if ([hitView isKindOfClass:[NSButton class]]) {
            return NO;
        }
        hitView = hitView.superview;
    }
    return YES;
}

- (void)workspaceTabGestureClicked:(NSGestureRecognizer*)gesture {
    NSView* view = gesture.view;
    if (!view || !_windowController || !_windowController.tabManager) return;

    NSNumber* workspaceIdNum = objc_getAssociatedObject(view, "workspaceId");
    if (!workspaceIdNum) return;

    int workspaceId = [workspaceIdNum intValue];

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
    CGFloat tabHeight = 28;  // Match plus button height for alignment
    CGFloat x = padding;
    CGFloat closeSize = 16;

    for (const auto& workspace : workspaces) {
        BOOL isActive = (activeWorkspace && workspace->id == activeWorkspace->id);
        int workspaceId = workspace->id;

        // Create draggable container view for the workspace tab
        DraggableWorkspaceTabView* tabContainer = [[DraggableWorkspaceTabView alloc] init];
        tabContainer.workspaceId = workspaceId;
        tabContainer.sidebarView = self;
        tabContainer.wantsLayer = YES;
        tabContainer.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        tabContainer.layer.backgroundColor = isActive ? [DSColors surfaceActive].CGColor : [NSColor clearColor].CGColor;

        // Calculate total width: padding + dot + gap + text + gap + close button + padding
        NSString* title = [NSString stringWithUTF8String:workspace->name.c_str()];
        NSDictionary* attrs = @{NSFontAttributeName: DSFont(DSFontStyleBodyMedium)};
        CGFloat textWidth = [title sizeWithAttributes:attrs].width;
        CGFloat innerPadding = 10;
        CGFloat dotSize = 8;
        CGFloat dotGap = 6;
        CGFloat totalWidth = innerPadding + dotSize + dotGap + textWidth + 6 + closeSize + innerPadding;
        totalWidth = MAX(80, totalWidth);

        // Position tabs to align with plus button - offset down from center
        CGFloat containerHeight = _workspaceTabsContainer.bounds.size.height;
        CGFloat tabY = (containerHeight - tabHeight) / 2;
        tabContainer.frame = NSMakeRect(x, tabY, totalWidth, tabHeight);

        // Color dot
        NSView* colorDot = [[NSView alloc] initWithFrame:NSMakeRect(
            innerPadding, (tabHeight - dotSize) / 2, dotSize, dotSize)];
        colorDot.wantsLayer = YES;
        colorDot.layer.cornerRadius = dotSize / 2;
        colorDot.layer.backgroundColor = NSColorFromHex(workspace->color).CGColor;
        [tabContainer addSubview:colorDot];

        // Title label (offset by dot + gap)
        CGFloat labelX = innerPadding + dotSize + dotGap;
        NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(
            labelX, (tabHeight - 18) / 2, textWidth + 4, 18)];
        label.stringValue = title;
        label.font = DSFont(DSFontStyleBodyMedium);
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

        // Make the container clickable using gesture recognizer
        NSClickGestureRecognizer* clickGesture = [[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(workspaceTabGestureClicked:)];
        clickGesture.numberOfClicksRequired = 1;
        clickGesture.delegate = self;
        objc_setAssociatedObject(tabContainer, "workspaceId", @(workspaceId), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [tabContainer addGestureRecognizer:clickGesture];

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

    // Bookmark Space
    NSMenuItem* bookmarkItem = [[NSMenuItem alloc] initWithTitle:@"Bookmark Space"
                                                           action:@selector(bookmarkWorkspace:)
                                                    keyEquivalent:@""];
    bookmarkItem.target = self;
    bookmarkItem.tag = workspaceId;
    [menu addItem:bookmarkItem];

    [menu addItem:[NSMenuItem separatorItem]];

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

    // Change Color submenu
    NSMenuItem* colorItem = [[NSMenuItem alloc] initWithTitle:@"Change Color" action:nil keyEquivalent:@""];
    NSMenu* colorMenu = [[NSMenu alloc] initWithTitle:@"Change Color"];

    NSArray* colorNames = @[@"Blue", @"Red", @"Green", @"Orange", @"Purple", @"Pink", @"Teal", @"Yellow"];
    for (int i = 0; i < 8; i++) {
        NSMenuItem* ci = [[NSMenuItem alloc] initWithTitle:colorNames[i]
                                                    action:@selector(changeWorkspaceColor:)
                                             keyEquivalent:@""];
        ci.target = self;
        ci.tag = workspaceId;
        ci.representedObject = @(i);

        // Create color swatch image
        NSImage* swatch = [[NSImage alloc] initWithSize:NSMakeSize(12, 12)];
        [swatch lockFocus];
        NSColor* swatchColor = NSColorFromHex(WorkspaceColors::ForIndex(i));
        [swatchColor setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(0, 0, 12, 12)] fill];
        [swatch unlockFocus];
        ci.image = swatch;

        [colorMenu addItem:ci];
    }
    colorItem.submenu = colorMenu;
    [menu addItem:colorItem];

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

    // Close Others (only show if more than 1 workspace)
    BOOL hasMultipleWorkspaces = _windowController && _windowController.tabManager &&
                                  _windowController.tabManager->GetWorkspaces().size() > 1;
    if (hasMultipleWorkspaces) {
        NSMenuItem* closeOthersItem = [[NSMenuItem alloc] initWithTitle:@"Close Others"
                                                                  action:@selector(closeOtherWorkspaces:)
                                                           keyEquivalent:@""];
        closeOthersItem.target = self;
        closeOthersItem.tag = workspaceId;
        [menu addItem:closeOthersItem];
    }

    // Close All
    NSMenuItem* closeAllItem = [[NSMenuItem alloc] initWithTitle:@"Close All"
                                                           action:@selector(closeAllWorkspaces:)
                                                    keyEquivalent:@""];
    closeAllItem.target = self;
    closeAllItem.tag = workspaceId;
    [menu addItem:closeAllItem];

    return menu;
}

- (void)closeWorkspace:(DSIconButton*)sender {
    [self deleteWorkspaceById:(int)sender.tag];
}

- (void)otherMouseDown:(NSEvent*)event {
    // Middle-click (button 3) closes workspace tabs
    if (event.buttonNumber == 2) {
        NSPoint location = [self convertPoint:event.locationInWindow fromView:nil];
        NSPoint containerLocation = [_workspaceTabsContainer convertPoint:location fromView:self];

        // Check if click is on a workspace tab
        for (NSView* tabView in _workspaceTabs) {
            if (NSPointInRect(containerLocation, tabView.frame)) {
                NSNumber* workspaceIdNum = objc_getAssociatedObject(tabView, "workspaceId");
                if (workspaceIdNum) {
                    [self deleteWorkspaceById:workspaceIdNum.intValue];
                }
                return;
            }
        }
    }
    [super otherMouseDown:event];
}

- (void)deleteWorkspace:(NSMenuItem*)sender {
    [self deleteWorkspaceById:(int)sender.tag];
}

- (void)closeOtherWorkspaces:(NSMenuItem*)sender {
    if (!_windowController || !_windowController.tabManager) return;

    int keepWorkspaceId = (int)sender.tag;

    // Collect IDs of workspaces to delete (can't modify while iterating)
    std::vector<int> toDelete;
    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        if (workspace->id != keepWorkspaceId) {
            toDelete.push_back(workspace->id);
        }
    }

    // Switch to the workspace we're keeping first
    _windowController.tabManager->SetActiveWorkspace(keepWorkspaceId);

    // Delete all other workspaces
    for (int wsId : toDelete) {
        [self performWorkspaceDeletion:wsId];
    }

    [self reloadWorkspaceTabs];
    [self reloadTabs];
}

- (void)closeAllWorkspaces:(NSMenuItem*)sender {
    (void)sender;
    if (!_windowController || !_windowController.tabManager) return;

    // Collect all workspace IDs
    std::vector<int> toDelete;
    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        toDelete.push_back(workspace->id);
    }

    // Create a new workspace first
    Workspace* newWs = _windowController.tabManager->CreateWorkspace("");
    _windowController.tabManager->SetActiveWorkspace(newWs->id);

    // Delete all old workspaces
    for (int wsId : toDelete) {
        [self performWorkspaceDeletion:wsId];
    }

    // Create a default tab in the new workspace
    [_windowController createNewTab:@""];

    [self reloadWorkspaceTabs];
    [self reloadTabs];
}

- (void)changeWorkspaceColor:(NSMenuItem*)sender {
    if (!_windowController || !_windowController.tabManager) return;

    int workspaceId = (int)sender.tag;
    int colorIndex = [sender.representedObject intValue];

    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        if (workspace->id == workspaceId) {
            workspace->color = WorkspaceColors::ForIndex(colorIndex);
            break;
        }
    }

    [self reloadWorkspaceTabs];
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
    for (const auto& workspace : _windowController.tabManager->GetWorkspaces()) {
        if (workspace->id == workspaceId) {
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

- (void)bookmarkWorkspace:(NSMenuItem*)sender {
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

    // Filter valid tabs (skip empty, about:blank, chrome://)
    std::vector<Tab*> validTabs;
    for (const auto& tab : workspace->tabs) {
        if (tab->url.empty()) continue;
        if (tab->url == "about:blank") continue;
        if (tab->url.find("chrome://") == 0) continue;
        if (tab->url.find("chrome-extension://") == 0) continue;
        validTabs.push_back(tab.get());
    }

    if (validTabs.empty()) {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"No Tabs to Bookmark";
        alert.informativeText = @"This space has no tabs with valid URLs to bookmark.";
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:_windowController.window completionHandler:nil];
        return;
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    // Sanitize folder name
    NSString* rawName = [NSString stringWithUTF8String:workspace->name.c_str()];
    NSString* folderName = [[rawName stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]] length] > 0 ? rawName : @"Untitled";

    // Check if folder already exists
    std::vector<std::string> existingFolders = bookmarks->GetFolders();
    bool folderExists = std::find(existingFolders.begin(), existingFolders.end(),
                                   [folderName UTF8String]) != existingFolders.end();

    if (folderExists) {
        // Ask user: Merge, New Folder, or Cancel
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"Folder Already Exists";
        alert.informativeText = [NSString stringWithFormat:
            @"A bookmark folder named \"%@\" already exists.", folderName];
        [alert addButtonWithTitle:@"Merge"];
        [alert addButtonWithTitle:@"New Folder"];
        [alert addButtonWithTitle:@"Cancel"];

        __weak SidebarView* weakSelf = self;
        NSString* baseFolderName = folderName;
        [alert beginSheetModalForWindow:_windowController.window
                      completionHandler:^(NSModalResponse response) {
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;

            if (response == NSAlertThirdButtonReturn) return;  // Cancel

            NSString* finalFolder = baseFolderName;
            if (response == NSAlertSecondButtonReturn) {
                // Create unique name
                int suffix = 2;
                while (std::find(existingFolders.begin(), existingFolders.end(),
                                 [finalFolder UTF8String]) != existingFolders.end()) {
                    finalFolder = [NSString stringWithFormat:@"%@ (%d)", baseFolderName, suffix++];
                }
            }

            [strongSelf addBookmarksToFolder:finalFolder tabs:validTabs];
        }];
    } else {
        [self addBookmarksToFolder:folderName tabs:validTabs];
    }
}

- (void)addBookmarksToFolder:(NSString*)folderName tabs:(const std::vector<Tab*>&)tabs {
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    int addedCount = 0;
    for (Tab* tab : tabs) {
        bookmarks->AddBookmark(tab->url, tab->title, [folderName UTF8String]);
        addedCount++;
    }

    // Show confirmation
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Space Bookmarked";
    alert.informativeText = [NSString stringWithFormat:
        @"Added %d bookmark%@ to folder \"%@\".",
        addedCount,
        addedCount == 1 ? @"" : @"s",
        folderName];
    [alert addButtonWithTitle:@"OK"];
    [alert beginSheetModalForWindow:_windowController.window completionHandler:nil];

    // Refresh bookmarks panel if visible
    if (_activePanel == SidebarPanelFavorites) {
        [self reloadBookmarks];
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
    _downloadsPanelContainer.hidden = YES;

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
        case SidebarPanelDownloads:
            _downloadsPanelContainer.hidden = NO;
            [self reloadDownloads];
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
    CGFloat topPadding = [DSSpacing sm];  // Small padding below workspace row
    CGFloat totalHeight = workspace->tabs.size() * rowHeight + topPadding;
    CGFloat minHeight = _tabScrollView.bounds.size.height;

    _tabContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(totalHeight, minHeight));

    CGFloat y = topPadding;
    int activeTabId = workspace->GetActiveTab() ? workspace->GetActiveTab()->id : -1;

    for (const auto& tab : workspace->tabs) {
        TabRowView* row = [[TabRowView alloc] initWithFrame:NSMakeRect(0, y, contentWidth, rowHeight)];
        row.tabId = tab->id;
        row.title = [NSString stringWithUTF8String:tab->title.c_str()];
        row.isSelected = (tab->id == activeTabId);
        row.isLoading = tab->is_loading;
        row.isPinned = tab->is_pinned;
        row.isMuted = tab->is_muted;
        row.sidebarView = self;

        NSString* url = [NSString stringWithUTF8String:tab->url.c_str()];

        // Use gear icon for orbfox:// internal pages
        if ([url hasPrefix:@"orbfox://"]) {
            NSImageSymbolConfiguration* config = [NSImageSymbolConfiguration
                configurationWithPointSize:14 weight:NSFontWeightMedium];
            NSImage* gearIcon = [[NSImage imageWithSystemSymbolName:@"gearshape.fill"
                                           accessibilityDescription:@"Settings"]
                                 imageWithSymbolConfiguration:config];
            if (gearIcon) {
                [gearIcon setTemplate:YES];
                row.favicon = gearIcon;
            }
        } else if (!tab->favicon_data.empty()) {
            NSData* faviconData = [NSData dataWithBytes:tab->favicon_data.data()
                                                 length:tab->favicon_data.size()];
            NSImage* favicon = [[NSImage alloc] initWithData:faviconData];
            if (favicon) {
                row.favicon = favicon;
                // Cache favicon by domain for use in bookmarks/history
                CacheFavicon(url, favicon);
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

    // Cache title by URL for bookmarks
    if (title.length > 0 && _windowController) {
        Tab* tab = _windowController.tabManager->GetTabById(tabId);
        if (tab) {
            NSString* url = [NSString stringWithUTF8String:tab->url.c_str()];
            CacheTitle(url, title);

            // Update bookmark titles if viewing bookmarks panel
            if (_activePanel == SidebarPanelFavorites) {
                [self updateBookmarkTitlesForUrl:url title:title];
            }
        }
    }
}

- (void)updateTab:(int)tabId faviconData:(NSData*)faviconData {
    if (!faviconData || faviconData.length == 0) return;

    // Get URL to check if it's an internal page
    NSString* cachedUrl = nil;
    if (_windowController) {
        Tab* tab = _windowController.tabManager->GetTabById(tabId);
        if (tab) {
            cachedUrl = [NSString stringWithUTF8String:tab->url.c_str()];
        }
    }

    // Don't update favicon for orbfox:// URLs - they use gear icon
    if ([cachedUrl hasPrefix:@"orbfox://"]) {
        return;
    }

    NSImage* favicon = [[NSImage alloc] initWithData:faviconData];
    if (!favicon) return;

    // Cache by URL for bookmarks/history
    if (cachedUrl) {
        CacheFavicon(cachedUrl, favicon);
    }

    for (TabRowView* row in _tabRows) {
        if (row.tabId == tabId) {
            row.favicon = favicon;
            break;
        }
    }

    // Update bookmark rows if visible and favicon was cached
    if (_activePanel == SidebarPanelFavorites && cachedUrl) {
        [self updateBookmarkFaviconsForUrl:cachedUrl favicon:favicon];
    }
}

- (void)updateTabWithGearIcon:(int)tabId {
    NSImageSymbolConfiguration* config = [NSImageSymbolConfiguration
        configurationWithPointSize:14 weight:NSFontWeightMedium];
    NSImage* gearIcon = [[NSImage imageWithSystemSymbolName:@"gearshape.fill"
                                   accessibilityDescription:@"Settings"]
                         imageWithSymbolConfiguration:config];
    if (!gearIcon) return;

    [gearIcon setTemplate:YES];

    for (TabRowView* row in _tabRows) {
        if (row.tabId == tabId) {
            row.favicon = gearIcon;
            break;
        }
    }
}

- (void)updateBookmarkFaviconsForUrl:(NSString*)url favicon:(NSImage*)favicon {
    NSString* domain = GetDomainFromURL(url);
    if (!domain) return;

    for (NSView* subview in _bookmarksContainer.subviews) {
        if ([subview isKindOfClass:[DSRow class]]) {
            DSRow* row = (DSRow*)subview;
            // Check if this bookmark's URL matches the domain
            NSString* bmUrl = objc_getAssociatedObject(row, "bookmarkUrl");
            if (bmUrl) {
                NSString* bmDomain = GetDomainFromURL(bmUrl);
                if (bmDomain && [bmDomain isEqualToString:domain]) {
                    row.icon = favicon;
                }
            }
        }
    }
}

- (void)updateBookmarkTitlesForUrl:(NSString*)url title:(NSString*)title {
    NSString* domain = GetDomainFromURL(url);
    if (!domain || !title) return;

    for (NSView* subview in _bookmarksContainer.subviews) {
        if ([subview isKindOfClass:[DSRow class]]) {
            DSRow* row = (DSRow*)subview;
            NSString* bmUrl = objc_getAssociatedObject(row, "bookmarkUrl");
            if (bmUrl) {
                NSString* bmDomain = GetDomainFromURL(bmUrl);
                if (bmDomain && [bmDomain isEqualToString:domain]) {
                    // Only update if the current title looks like a URL/domain
                    NSString* currentTitle = row.title;
                    if ([currentTitle containsString:@"."] && ![currentTitle containsString:@" "]) {
                        row.title = title;
                    }
                }
            }
        }
    }
}

#pragma mark - Bookmarks

- (void)addBookmarkClicked:(id)sender {
    NSLog(@"addBookmarkClicked called");

    // Create menu
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Add"];

    NSMenuItem* addBookmarkItem = [[NSMenuItem alloc] initWithTitle:@"Add Bookmark..."
                                                             action:@selector(showAddBookmarkPopover:)
                                                      keyEquivalent:@""];
    addBookmarkItem.target = self;
    addBookmarkItem.image = [NSImage imageWithSystemSymbolName:@"bookmark" accessibilityDescription:nil];
    [menu addItem:addBookmarkItem];

    NSMenuItem* newFolderItem = [[NSMenuItem alloc] initWithTitle:@"New Folder..."
                                                           action:@selector(newFolderFromMenuClicked:)
                                                    keyEquivalent:@""];
    newFolderItem.target = self;
    newFolderItem.image = [NSImage imageWithSystemSymbolName:@"folder.badge.plus" accessibilityDescription:nil];
    [menu addItem:newFolderItem];

    // Show menu at button location
    NSView* button = (NSView*)sender;
    NSPoint point = NSMakePoint(0, button.bounds.size.height + 2);
    [menu popUpMenuPositioningItem:nil atLocation:point inView:button];
}

- (void)showAddBookmarkPopover:(id)sender {
    (void)sender;
    [self showAddBookmarkPopoverWithUrl:nil title:nil];
}

- (void)showAddBookmarkPopoverWithUrl:(NSString*)url title:(NSString*)title {
    // Create the add bookmark popover if needed
    if (!_addBookmarkPopover) {
        _addBookmarkPopover = [[AddBookmarkPopoverController alloc] init];
        _addBookmarkPopover.sidebarView = self;
    }

    NSView* anchorView = _addBookmarkButton ?: _bookmarksPanelContainer;
    [_addBookmarkPopover showRelativeToView:anchorView withUrl:url title:title];
}

- (void)newFolderClicked:(id)sender {
    (void)sender;
    NSLog(@"newFolderClicked: method called");

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) {
        NSLog(@"newFolderClicked: BookmarkStorage is nil!");
        return;
    }

    // Get default folder name
    int nextNum = bookmarks->GetNextFolderNumber();
    NSString* defaultName = [NSString stringWithFormat:@"Collection %d", nextNum];
    NSLog(@"newFolderClicked: defaultName = %@", defaultName);

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

    NSModalResponse response = [alert runModal];
    NSLog(@"newFolderClicked: response = %ld, NSAlertFirstButtonReturn = %ld", (long)response, (long)NSAlertFirstButtonReturn);

    if (response == NSAlertFirstButtonReturn) {
        NSString* folderName = [input.stringValue stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        NSLog(@"newFolderClicked: folderName after trim = '%@'", folderName);

        if (folderName.length == 0) {
            folderName = defaultName;
        }

        // Check if folder already exists
        bool exists = bookmarks->FolderExists([folderName UTF8String]);
        NSLog(@"newFolderClicked: folder exists = %d", exists);

        if (exists) {
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
        NSLog(@"newFolderClicked: Creating folder: %@", folderName);
        bool created = bookmarks->CreateFolder([folderName UTF8String]);
        NSLog(@"newFolderClicked: Folder created result = %d", created);

        [self reloadBookmarks];
        NSLog(@"newFolderClicked: reloadBookmarks called");
    } else {
        NSLog(@"newFolderClicked: User cancelled or other response");
    }
}

- (void)newFolderFromMenuClicked:(id)sender {
    [self newFolderClicked:sender];
}

- (void)reloadBookmarks {
    // Remove all subviews except the drop indicator
    NSView* dropIndicator = _bookmarksContainer.dropIndicator;
    for (NSView* subview in _bookmarksContainer.subviews.copy) {
        if (subview != dropIndicator) {
            [subview removeFromSuperview];
        }
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    std::vector<Bookmark> allEntries = bookmarks->GetAllBookmarks();
    std::vector<std::string> folders = bookmarks->GetFolders();
    CGFloat contentWidth = _bookmarksContainer.bounds.size.width;
    CGFloat y = 0;
    CGFloat rowHeight = 44;
    CGFloat folderHeaderHeight = 36;
    CGFloat indentWidth = [DSSpacing md];

    if (allEntries.empty() && folders.empty()) {
        // Show empty state only if both bookmarks and folders are empty
        NSTextField* emptyLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
            [DSSpacing md], y + 20, contentWidth - [DSSpacing xl], 40)];
        emptyLabel.stringValue = @"No bookmarks yet.\nClick + to add a bookmark.";
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

    // 1. Display root bookmarks first (folder == "")
    for (const auto& entry : allEntries) {
        if (!entry.folder.empty()) continue;
        DSRow* row = [self createBookmarkRowForEntry:entry atY:y width:contentWidth indent:0];
        [_bookmarksContainer addSubview:row];
        y += rowHeight;
    }

    // 2. Display folders with their bookmarks
    for (const auto& folderName : folders) {
        NSString* folder = [NSString stringWithUTF8String:folderName.c_str()];
        BOOL isCollapsed = [_collapsedFolders containsObject:folder];

        // Create folder header
        NSView* folderHeader = [self createFolderHeader:folder
                                                    atY:y
                                                  width:contentWidth
                                            isCollapsed:isCollapsed];
        [_bookmarksContainer addSubview:folderHeader];
        y += folderHeaderHeight;

        // Add bookmarks under this folder (if not collapsed)
        if (!isCollapsed) {
            std::vector<Bookmark> folderBookmarks = bookmarks->GetBookmarksInFolder(folderName);
            for (const auto& entry : folderBookmarks) {
                DSRow* row = [self createBookmarkRowForEntry:entry atY:y width:contentWidth indent:indentWidth];
                [_bookmarksContainer addSubview:row];
                y += rowHeight;
            }
        }
    }

    _bookmarksContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _bookmarksScrollView.bounds.size.height));
}

- (DSRow*)createBookmarkRowForEntry:(const Bookmark&)entry
                                atY:(CGFloat)y
                              width:(CGFloat)width
                             indent:(CGFloat)indent {
    CGFloat rowHeight = 44;
    DSRow* row = [[DSRow alloc] initWithFrame:NSMakeRect(
        [DSSpacing xs] + indent, y, width - [DSSpacing sm] - indent, rowHeight)];
    row.autoresizingMask = NSViewWidthSizable;

    NSString* urlStr = [NSString stringWithUTF8String:entry.url.c_str()];
    NSString* folderStr = [NSString stringWithUTF8String:entry.folder.c_str()];

    // Title display priority: nickname (title field) > cached page title > domain name
    NSString* title;
    if (!entry.title.empty()) {
        NSString* savedTitle = [NSString stringWithUTF8String:entry.title.c_str()];
        // Check if saved title looks like a URL/domain (auto-generated)
        if ([savedTitle containsString:@"."] && ![savedTitle containsString:@" "]) {
            // Try to get cached page title instead
            NSString* cachedTitle = GetCachedTitle(urlStr);
            title = cachedTitle ?: savedTitle;
        } else {
            title = savedTitle;
        }
    } else {
        // Try cached page title first
        NSString* cachedTitle = GetCachedTitle(urlStr);
        if (cachedTitle) {
            title = cachedTitle;
        } else {
            // Fall back to domain name
            NSURL* url = [NSURL URLWithString:urlStr];
            NSString* host = url.host;
            if (host) {
                // Remove "www." prefix if present
                if ([host hasPrefix:@"www."]) {
                    host = [host substringFromIndex:4];
                }
                title = host;
            } else {
                title = urlStr;
            }
        }
    }

    row.title = title;
    row.showsCloseButton = YES;

    // Set favicon from cache if available, otherwise show globe icon
    NSImage* favicon = GetCachedFavicon(urlStr);
    if (favicon) {
        row.icon = favicon;
    } else {
        // Default globe icon for bookmarks without cached favicon
        NSImage* globeIcon = [NSImage imageWithSystemSymbolName:@"globe" accessibilityDescription:nil];
        row.icon = globeIcon;
    }

    __weak SidebarView* weakSelf = self;
    NSString* urlCopy = urlStr;
    NSString* titleCopy = title;
    NSString* folderCopy = folderStr;
    int64_t bookmarkId = entry.id;

    // Associate bookmark ID and URL with row for later lookup
    objc_setAssociatedObject(row, "bookmarkId", @(bookmarkId), OBJC_ASSOCIATION_RETAIN);
    objc_setAssociatedObject(row, "bookmarkUrl", urlStr, OBJC_ASSOCIATION_RETAIN);

    // Single click = select bookmark
    row.onClick = ^{
        SidebarView* strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf selectBookmark:bookmarkId];
    };

    // Double click = open in new tab
    row.onDoubleClick = ^{
        SidebarView* strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf.windowController createNewTab:urlCopy];
        strongSelf->_activePanel = SidebarPanelTabs;
        [strongSelf updateIconSelection];
        [strongSelf updatePanelVisibility];
    };

    // Right click = context menu
    row.onRightClick = ^(NSEvent* event) {
        SidebarView* strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf selectBookmark:bookmarkId];
        NSMenu* menu = [strongSelf createBookmarkContextMenu:bookmarkId
                                                         url:urlCopy
                                                       title:titleCopy
                                                      folder:folderCopy];
        [NSMenu popUpContextMenu:menu withEvent:event forView:row];
    };

    // Middle click = open in background tab
    row.onMiddleClick = ^{
        SidebarView* strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf.windowController openUrlInBackgroundTab:urlCopy];
    };

    row.onClose = ^{
        SidebarView* strongSelf = weakSelf;
        if (!strongSelf) return;
        BookmarkStorage* bm = GetBookmarkStorage();
        if (bm) {
            bm->DeleteBookmark(bookmarkId);
            strongSelf->_selectedBookmarkId = 0;
            [strongSelf reloadBookmarks];
        }
    };

    // Set initial selection state
    row.isSelected = (bookmarkId == _selectedBookmarkId);

    // Enable drag & drop
    row.isDraggable = YES;
    row.dragType = kBookmarkPasteboardType;
    row.dragData = @{
        @"id": @(bookmarkId),
        @"folder": folderCopy ?: @""
    };

    return row;
}

#pragma mark - Bookmark Selection

- (void)selectBookmark:(int64_t)bookmarkId {
    _selectedBookmarkId = bookmarkId;
    [self updateBookmarkSelection];
}

- (void)updateBookmarkSelection {
    for (NSView* subview in _bookmarksContainer.subviews) {
        if ([subview isKindOfClass:[DSRow class]]) {
            DSRow* row = (DSRow*)subview;
            NSNumber* rowIdNum = objc_getAssociatedObject(row, "bookmarkId");
            if (rowIdNum) {
                int64_t rowId = [rowIdNum longLongValue];
                row.isSelected = (rowId == _selectedBookmarkId);
            }
        }
    }
}

#pragma mark - Bookmark Context Menu

- (NSMenu*)createBookmarkContextMenu:(int64_t)bookmarkId
                                 url:(NSString*)url
                               title:(NSString*)title
                              folder:(NSString*)folder {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Bookmark"];

    // Open (in current tab)
    NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                      action:@selector(openBookmarkInCurrentTab:)
                                               keyEquivalent:@""];
    openItem.target = self;
    objc_setAssociatedObject(openItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openItem];

    // Open in New Tab
    NSMenuItem* openNewTabItem = [[NSMenuItem alloc] initWithTitle:@"Open in New Tab"
                                                            action:@selector(openBookmarkInNewTab:)
                                                     keyEquivalent:@""];
    openNewTabItem.target = self;
    objc_setAssociatedObject(openNewTabItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openNewTabItem];

    // Open in Background Tab
    NSMenuItem* openBackgroundItem = [[NSMenuItem alloc] initWithTitle:@"Open in Background Tab"
                                                                action:@selector(openBookmarkInBackgroundTab:)
                                                         keyEquivalent:@""];
    openBackgroundItem.target = self;
    objc_setAssociatedObject(openBackgroundItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openBackgroundItem];

    [menu addItem:[NSMenuItem separatorItem]];

    // Edit Bookmark
    NSMenuItem* editItem = [[NSMenuItem alloc] initWithTitle:@"Edit Bookmark..."
                                                      action:@selector(editBookmarkFromMenu:)
                                               keyEquivalent:@""];
    editItem.target = self;
    editItem.tag = (NSInteger)bookmarkId;
    objc_setAssociatedObject(editItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    objc_setAssociatedObject(editItem, "title", title, OBJC_ASSOCIATION_RETAIN);
    objc_setAssociatedObject(editItem, "folder", folder, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:editItem];

    // Move to Folder submenu
    NSMenuItem* moveItem = [[NSMenuItem alloc] initWithTitle:@"Move to Folder"
                                                      action:nil
                                               keyEquivalent:@""];
    moveItem.submenu = [self createMoveToFolderSubmenu:bookmarkId currentFolder:folder];
    [menu addItem:moveItem];

    [menu addItem:[NSMenuItem separatorItem]];

    // Delete
    NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete"
                                                        action:@selector(deleteBookmarkFromMenu:)
                                                 keyEquivalent:@""];
    deleteItem.target = self;
    deleteItem.tag = (NSInteger)bookmarkId;
    [menu addItem:deleteItem];

    return menu;
}

- (NSMenu*)createMoveToFolderSubmenu:(int64_t)bookmarkId currentFolder:(NSString*)currentFolder {
    NSMenu* submenu = [[NSMenu alloc] initWithTitle:@"Move to Folder"];

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return submenu;

    // "No Folder" option (root level)
    NSMenuItem* rootItem = [[NSMenuItem alloc] initWithTitle:@"No Folder"
                                                      action:@selector(moveBookmarkToFolder:)
                                               keyEquivalent:@""];
    rootItem.target = self;
    rootItem.tag = (NSInteger)bookmarkId;
    objc_setAssociatedObject(rootItem, "folder", @"", OBJC_ASSOCIATION_RETAIN);
    if (!currentFolder || currentFolder.length == 0) {
        rootItem.state = NSControlStateValueOn;
    }
    [submenu addItem:rootItem];

    // List all folders
    std::vector<std::string> folders = bookmarks->GetFolders();
    if (!folders.empty()) {
        [submenu addItem:[NSMenuItem separatorItem]];
        for (const auto& folderName : folders) {
            NSString* folder = [NSString stringWithUTF8String:folderName.c_str()];
            NSMenuItem* folderItem = [[NSMenuItem alloc] initWithTitle:folder
                                                                action:@selector(moveBookmarkToFolder:)
                                                         keyEquivalent:@""];
            folderItem.target = self;
            folderItem.tag = (NSInteger)bookmarkId;
            objc_setAssociatedObject(folderItem, "folder", folder, OBJC_ASSOCIATION_RETAIN);
            if ([folder isEqualToString:currentFolder]) {
                folderItem.state = NSControlStateValueOn;
            }
            [submenu addItem:folderItem];
        }
    }

    return submenu;
}

#pragma mark - Bookmark Menu Actions

- (void)openBookmarkInCurrentTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInCurrentTab:url];
}

- (void)openBookmarkInNewTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInNewTab:url];
}

- (void)openBookmarkInBackgroundTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInBackgroundTab:url];
}

- (void)editBookmarkFromMenu:(NSMenuItem*)sender {
    int64_t bookmarkId = (int64_t)sender.tag;
    NSString* url = objc_getAssociatedObject(sender, "url");
    NSString* title = objc_getAssociatedObject(sender, "title");
    NSString* folder = objc_getAssociatedObject(sender, "folder");

    // Show edit dialog
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Edit Bookmark";
    alert.informativeText = url;
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];

    // Create accessory view with name and folder fields
    NSView* accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 70)];

    NSTextField* nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 48, 50, 18)];
    nameLabel.stringValue = @"Name:";
    nameLabel.bezeled = NO;
    nameLabel.drawsBackground = NO;
    nameLabel.editable = NO;
    [accessoryView addSubview:nameLabel];

    NSTextField* nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(55, 45, 220, 24)];
    nameField.stringValue = title ?: @"";
    [accessoryView addSubview:nameField];

    NSTextField* folderLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 18, 50, 18)];
    folderLabel.stringValue = @"Folder:";
    folderLabel.bezeled = NO;
    folderLabel.drawsBackground = NO;
    folderLabel.editable = NO;
    [accessoryView addSubview:folderLabel];

    NSPopUpButton* folderPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(55, 13, 220, 26) pullsDown:NO];
    [folderPicker addItemWithTitle:@"No Folder"];
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        std::vector<std::string> folders = bookmarks->GetFolders();
        if (!folders.empty()) {
            [[folderPicker menu] addItem:[NSMenuItem separatorItem]];
            for (const auto& f : folders) {
                [folderPicker addItemWithTitle:[NSString stringWithUTF8String:f.c_str()]];
            }
        }
    }
    if (folder && folder.length > 0) {
        [folderPicker selectItemWithTitle:folder];
    }
    [accessoryView addSubview:folderPicker];

    alert.accessoryView = accessoryView;
    [alert.window makeFirstResponder:nameField];

    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        NSString* newTitle = nameField.stringValue;
        NSString* newFolder = @"";
        if ([folderPicker indexOfSelectedItem] > 0) {
            newFolder = [folderPicker titleOfSelectedItem];
        }

        if (bookmarks) {
            bookmarks->UpdateBookmark(bookmarkId, [newTitle UTF8String], [newFolder UTF8String]);
            [self reloadBookmarks];
        }
    }
}

- (void)moveBookmarkToFolder:(NSMenuItem*)sender {
    int64_t bookmarkId = (int64_t)sender.tag;
    NSString* folder = objc_getAssociatedObject(sender, "folder");

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    bookmarks->MoveBookmark(bookmarkId, [folder UTF8String], 0);
    [self reloadBookmarks];
}

- (void)deleteBookmarkFromMenu:(NSMenuItem*)sender {
    int64_t bookmarkId = (int64_t)sender.tag;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        bookmarks->DeleteBookmark(bookmarkId);
        _selectedBookmarkId = 0;
        [self reloadBookmarks];
    }
}

- (void)showEditBookmarkDialog:(int64_t)bookmarkId {
    // Create the bookmark popover if needed (reuse for both add and edit)
    if (!_addBookmarkPopover) {
        _addBookmarkPopover = [[AddBookmarkPopoverController alloc] init];
        _addBookmarkPopover.sidebarView = self;
    }

    // Show as sheet since we don't have an anchor view for the edit action
    [_addBookmarkPopover showEditAsSheetInWindow:_windowController.window bookmarkId:bookmarkId];
}

#pragma mark - History Context Menu

- (NSMenu*)createHistoryContextMenu:(NSString*)url entryId:(int64_t)entryId {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"History"];

    // Open (in current tab)
    NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                      action:@selector(openHistoryInCurrentTab:)
                                               keyEquivalent:@""];
    openItem.target = self;
    objc_setAssociatedObject(openItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openItem];

    // Open in New Tab
    NSMenuItem* openNewTabItem = [[NSMenuItem alloc] initWithTitle:@"Open in New Tab"
                                                            action:@selector(openHistoryInNewTab:)
                                                     keyEquivalent:@""];
    openNewTabItem.target = self;
    objc_setAssociatedObject(openNewTabItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openNewTabItem];

    // Open in Background Tab
    NSMenuItem* openBackgroundItem = [[NSMenuItem alloc] initWithTitle:@"Open in Background Tab"
                                                                action:@selector(openHistoryInBackgroundTab:)
                                                         keyEquivalent:@""];
    openBackgroundItem.target = self;
    objc_setAssociatedObject(openBackgroundItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openBackgroundItem];

    [menu addItem:[NSMenuItem separatorItem]];

    // Copy URL
    NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy URL"
                                                      action:@selector(copyHistoryUrl:)
                                               keyEquivalent:@""];
    copyItem.target = self;
    objc_setAssociatedObject(copyItem, "url", url, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:copyItem];

    // Delete from History
    NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete from History"
                                                        action:@selector(deleteFromHistory:)
                                                 keyEquivalent:@""];
    deleteItem.target = self;
    deleteItem.tag = (NSInteger)entryId;
    [menu addItem:deleteItem];

    return menu;
}

- (void)openHistoryInCurrentTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInCurrentTab:url];
}

- (void)openHistoryInNewTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInNewTab:url];
}

- (void)openHistoryInBackgroundTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController openUrlInBackgroundTab:url];
}

- (void)copyHistoryUrl:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    [_windowController copyUrlToClipboard:url];
}

- (void)deleteFromHistory:(NSMenuItem*)sender {
    int64_t entryId = (int64_t)sender.tag;
    if (entryId > 0) {
        HistoryStorage* history = GetHistoryStorage();
        if (history) {
            history->DeleteEntry(entryId);
            [self reloadHistoryWithQuery:_historySearchQuery];
        }
    }
}

- (NSView*)createFolderHeader:(NSString*)folderName
                          atY:(CGFloat)y
                        width:(CGFloat)width
                  isCollapsed:(BOOL)isCollapsed {
    CGFloat headerHeight = 36;
    NSView* header = [[NSView alloc] initWithFrame:NSMakeRect(0, y, width, headerHeight)];
    header.wantsLayer = YES;
    header.layer.cornerRadius = [DSLayout cornerRadiusMedium];
    header.autoresizingMask = NSViewWidthSizable;

    CGFloat padding = [DSSpacing sm];
    CGFloat verticalCenter = (headerHeight - 16) / 2;  // Center 16px icons vertically

    // Chevron button (expand/collapse)
    NSString* chevronName = isCollapsed ? @"chevron.right" : @"chevron.down";
    DSIconButton* chevronBtn = [DSIconButton buttonWithIcon:chevronName tooltip:isCollapsed ? @"Expand" : @"Collapse"];
    chevronBtn.frame = NSMakeRect(padding - 4, (headerHeight - 20) / 2, 20, 20);
    chevronBtn.target = self;
    chevronBtn.action = @selector(folderHeaderClicked:);
    objc_setAssociatedObject(chevronBtn, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [header addSubview:chevronBtn];

    // Folder icon (gray, not blue)
    NSImageView* folderIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(padding + 20, verticalCenter, 16, 16)];
    folderIcon.image = [NSImage imageWithSystemSymbolName:@"folder.fill" accessibilityDescription:nil];
    folderIcon.contentTintColor = [DSColors textSecondary];
    [header addSubview:folderIcon];

    // Folder name label (aligned with icons)
    NSTextField* nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        padding + 42, verticalCenter - 1, width - padding - 80, 18)];
    nameLabel.stringValue = folderName;
    nameLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
    nameLabel.textColor = [DSColors textPrimary];
    nameLabel.bezeled = NO;
    nameLabel.drawsBackground = NO;
    nameLabel.editable = NO;
    nameLabel.selectable = NO;
    nameLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    nameLabel.autoresizingMask = NSViewWidthSizable;
    [header addSubview:nameLabel];

    // "Open All" button (right side, always visible, above click area)
    DSIconButton* openAllBtn = [DSIconButton buttonWithIcon:@"arrow.up.right.square" tooltip:@"Open all in new tabs"];
    openAllBtn.frame = NSMakeRect(width - padding - 24, (headerHeight - 20) / 2, 20, 20);
    openAllBtn.target = self;
    openAllBtn.action = @selector(openAllInFolder:);
    openAllBtn.autoresizingMask = NSViewMinXMargin;  // Anchor to right side
    objc_setAssociatedObject(openAllBtn, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [header addSubview:openAllBtn];

    // Right-click context menu
    NSMenu* contextMenu = [self createFolderContextMenu:folderName];
    header.menu = contextMenu;

    return header;
}

- (NSMenu*)createFolderContextMenu:(NSString*)folderName {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Folder"];

    NSMenuItem* openAllItem = [[NSMenuItem alloc] initWithTitle:@"Open All"
                                                         action:@selector(openAllInFolderMenuItem:)
                                                  keyEquivalent:@""];
    openAllItem.target = self;
    objc_setAssociatedObject(openAllItem, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:openAllItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* renameItem = [[NSMenuItem alloc] initWithTitle:@"Rename Folder"
                                                        action:@selector(renameFolder:)
                                                 keyEquivalent:@""];
    renameItem.target = self;
    objc_setAssociatedObject(renameItem, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:renameItem];

    NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete Folder"
                                                        action:@selector(deleteFolder:)
                                                 keyEquivalent:@""];
    deleteItem.target = self;
    objc_setAssociatedObject(deleteItem, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [menu addItem:deleteItem];

    return menu;
}

- (void)folderHeaderClicked:(id)sender {
    NSString* folderName = objc_getAssociatedObject(sender, "folderName");
    if (!folderName) return;

    if ([_collapsedFolders containsObject:folderName]) {
        [_collapsedFolders removeObject:folderName];
    } else {
        [_collapsedFolders addObject:folderName];
    }
    [self reloadBookmarks];
}

- (void)openAllInFolder:(DSIconButton*)sender {
    NSString* folderName = objc_getAssociatedObject(sender, "folderName");
    [self openAllBookmarksInFolder:folderName];
}

- (void)openAllInFolderMenuItem:(NSMenuItem*)sender {
    NSString* folderName = objc_getAssociatedObject(sender, "folderName");
    [self openAllBookmarksInFolder:folderName];
}

- (void)openAllBookmarksInFolder:(NSString*)folderName {
    if (!folderName) return;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    std::vector<Bookmark> folderBookmarks = bookmarks->GetBookmarksInFolder([folderName UTF8String]);
    for (const auto& entry : folderBookmarks) {
        [_windowController createNewTab:[NSString stringWithUTF8String:entry.url.c_str()]];
    }

    // Switch to tabs panel
    _activePanel = SidebarPanelTabs;
    [self updateIconSelection];
    [self updatePanelVisibility];
}

- (void)renameFolder:(NSMenuItem*)sender {
    NSString* oldName = objc_getAssociatedObject(sender, "folderName");
    if (!oldName) return;

    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Rename Folder";
    alert.informativeText = @"Enter a new name for this folder:";
    [alert addButtonWithTitle:@"Rename"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    input.stringValue = oldName;
    alert.accessoryView = input;

    __weak SidebarView* weakSelf = self;
    NSString* oldNameCopy = oldName;
    [alert beginSheetModalForWindow:_windowController.window completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            NSString* newName = input.stringValue;
            if (newName.length > 0 && ![newName isEqualToString:oldNameCopy]) {
                SidebarView* strongSelf = weakSelf;
                if (strongSelf) {
                    [strongSelf renameFolderFrom:oldNameCopy to:newName];
                }
            }
        }
    }];
}

- (void)renameFolderFrom:(NSString*)oldName to:(NSString*)newName {
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    // Update all bookmarks in the folder
    std::vector<Bookmark> folderBookmarks = bookmarks->GetBookmarksInFolder([oldName UTF8String]);
    for (const auto& entry : folderBookmarks) {
        bookmarks->UpdateBookmark(entry.id, entry.title, [newName UTF8String]);
    }

    // Update collapsed state
    if ([_collapsedFolders containsObject:oldName]) {
        [_collapsedFolders removeObject:oldName];
        [_collapsedFolders addObject:newName];
    }

    [self reloadBookmarks];
}

- (void)deleteFolder:(NSMenuItem*)sender {
    NSString* folderName = objc_getAssociatedObject(sender, "folderName");
    if (!folderName) return;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    std::vector<Bookmark> folderBookmarks = bookmarks->GetBookmarksInFolder([folderName UTF8String]);

    // If folder is empty, delete without confirmation
    if (folderBookmarks.empty()) {
        bookmarks->DeleteFolder([folderName UTF8String]);
        [_collapsedFolders removeObject:folderName];
        [self reloadBookmarks];
        return;
    }

    // Show confirmation for non-empty folders
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Delete Folder?";
    alert.informativeText = [NSString stringWithFormat:
        @"This will delete %lu bookmark%@ in the folder \"%@\".",
        folderBookmarks.size(),
        folderBookmarks.size() == 1 ? @"" : @"s",
        folderName];
    [alert addButtonWithTitle:@"Delete"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.alertStyle = NSAlertStyleWarning;

    __weak SidebarView* weakSelf = self;
    NSString* folderNameCopy = folderName;
    [alert beginSheetModalForWindow:_windowController.window completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            BookmarkStorage* bm = GetBookmarkStorage();
            if (bm) {
                // DeleteFolder handles both bookmarks and folder entry
                bm->DeleteFolder([folderNameCopy UTF8String]);
                [strongSelf->_collapsedFolders removeObject:folderNameCopy];
                [strongSelf reloadBookmarks];
            }
        }
    }];
}

#pragma mark - History

- (void)reloadHistory {
    [self reloadHistoryWithQuery:_historySearchQuery];
}

- (void)clearHistoryClicked:(id)sender {
    (void)sender;
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Clear Browsing History?";
    alert.informativeText = @"This will permanently delete all browsing history. This action cannot be undone.";
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:@"Clear History"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        HistoryStorage* history = GetHistoryStorage();
        if (history) {
            history->ClearAllHistory();
            [self reloadHistory];
        }
    }
}

- (void)reloadHistoryWithQuery:(NSString*)query {
    for (NSView* subview in _historyContainer.subviews.copy) {
        [subview removeFromSuperview];
    }

    HistoryStorage* history = GetHistoryStorage();
    if (!history) return;

    std::vector<HistoryEntry> entries;
    if (query && query.length > 0) {
        entries = history->SearchHistory([query UTF8String], 100);
    } else {
        entries = history->GetRecentHistory(100);
    }
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
            dateHeader.autoresizingMask = NSViewWidthSizable;
            [_historyContainer addSubview:dateHeader];
            y += 28;
            lastDateString = dateString;
        }

        // History row using DSHistoryRow
        DSHistoryRow* row = [[DSHistoryRow alloc] initWithFrame:NSMakeRect(
            [DSSpacing xs], y, contentWidth - [DSSpacing sm], rowHeight)];
        row.autoresizingMask = NSViewWidthSizable;
        row.title = [NSString stringWithUTF8String:entry.title.empty()
            ? entry.url.c_str() : entry.title.c_str()];
        row.url = [NSString stringWithUTF8String:entry.url.c_str()];
        row.time = [timeFormatter stringFromDate:visitDate];

        // Set favicon from cache if available
        NSImage* favicon = GetCachedFavicon(row.url);
        if (favicon) {
            row.icon = favicon;
        }

        __weak SidebarView* weakSelf = self;
        NSString* urlCopy = row.url;
        int64_t entryId = entry.id;
        row.onClick = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            // Open as new tab in active workspace
            [strongSelf.windowController createNewTab:urlCopy];
            strongSelf->_activePanel = SidebarPanelTabs;
            [strongSelf updateIconSelection];
            [strongSelf updatePanelVisibility];
        };

        row.onRightClick = ^(NSEvent* event) {
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            NSMenu* menu = [strongSelf createHistoryContextMenu:urlCopy entryId:entryId];
            [NSMenu popUpContextMenu:menu withEvent:event forView:row];
        };

        // Middle click = open in background tab
        row.onMiddleClick = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            [strongSelf.windowController openUrlInBackgroundTab:urlCopy];
        };

        [_historyContainer addSubview:row];
        y += rowHeight;
    }

    _historyContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _historyScrollView.bounds.size.height));
}

#pragma mark - NSTextFieldDelegate (History Search)

- (void)controlTextDidChange:(NSNotification*)notification {
    NSTextField* textField = notification.object;
    if (textField == _historySearchField) {
        _historySearchQuery = textField.stringValue;
        [self reloadHistoryWithQuery:_historySearchQuery];
    }
}

#pragma mark - Downloads

- (void)clearCompletedDownloads:(id)sender {
    (void)sender;
    DownloadManager::GetInstance().ClearCompleted();
    [self reloadDownloads];
}

- (NSString*)formatBytes:(int64_t)bytes {
    if (bytes < 0) {
        return @"Unknown";
    } else if (bytes < 1024) {
        return [NSString stringWithFormat:@"%lld B", bytes];
    } else if (bytes < 1024 * 1024) {
        return [NSString stringWithFormat:@"%.1f KB", bytes / 1024.0];
    } else if (bytes < 1024 * 1024 * 1024) {
        return [NSString stringWithFormat:@"%.1f MB", bytes / (1024.0 * 1024.0)];
    } else {
        return [NSString stringWithFormat:@"%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0)];
    }
}

- (NSString*)formatSpeed:(int64_t)bytesPerSec {
    if (bytesPerSec < 1024) {
        return [NSString stringWithFormat:@"%lld B/s", bytesPerSec];
    } else if (bytesPerSec < 1024 * 1024) {
        return [NSString stringWithFormat:@"%.1f KB/s", bytesPerSec / 1024.0];
    } else {
        return [NSString stringWithFormat:@"%.1f MB/s", bytesPerSec / (1024.0 * 1024.0)];
    }
}

- (void)reloadDownloads {
    // Clear selection since views are being recreated
    _selectedDownloadRow = nil;

    for (NSView* subview in _downloadsContainer.subviews.copy) {
        [subview removeFromSuperview];
    }

    std::vector<DownloadItem> downloads = DownloadManager::GetInstance().GetDownloads();
    CGFloat contentWidth = _downloadsContainer.bounds.size.width;
    CGFloat y = 0;
    CGFloat rowHeight = 72;  // Taller to fit progress bar

    if (downloads.empty()) {
        // Show empty state
        NSTextField* emptyLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
            [DSSpacing md], y + 20, contentWidth - [DSSpacing xl], 40)];
        emptyLabel.stringValue = @"No downloads yet.\nDownloaded files will appear here.";
        emptyLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
        emptyLabel.textColor = [DSColors textSecondary];
        emptyLabel.bezeled = NO;
        emptyLabel.drawsBackground = NO;
        emptyLabel.editable = NO;
        emptyLabel.selectable = NO;
        emptyLabel.alignment = NSTextAlignmentCenter;
        [_downloadsContainer addSubview:emptyLabel];
        return;
    }

    for (const auto& download : downloads) {
        // Create row container with right-click support
        DownloadRowView* row = [[DownloadRowView alloc] initWithFrame:NSMakeRect(
            [DSSpacing xs], y, contentWidth - [DSSpacing sm], rowHeight)];
        row.wantsLayer = YES;
        row.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        row.autoresizingMask = NSViewWidthSizable;
        row.downloadId = download.id;
        row.downloadPath = [NSString stringWithUTF8String:download.full_path.c_str()];
        // Use original_url for display/copy (the URL user sees), fallback to url
        std::string display_url = download.original_url.empty() ? download.url : download.original_url;
        row.downloadUrl = [NSString stringWithUTF8String:display_url.c_str()];
        row.isInProgress = (download.state == DownloadState::InProgress ||
                            download.state == DownloadState::Paused);
        row.isStopped = (download.state == DownloadState::Canceled ||
                         download.state == DownloadState::Interrupted);
        row.isComplete = (download.state == DownloadState::Complete);
        row.sidebarView = self;

        // Check if completed file still exists on disk
        BOOL fileMissing = NO;
        if (row.isComplete && row.downloadPath.length > 0) {
            NSFileManager* fm = [NSFileManager defaultManager];
            if (![fm fileExistsAtPath:row.downloadPath]) {
                fileMissing = YES;
                row.isFileMissing = YES;
            }
        }

        // Filename
        NSString* filename = [NSString stringWithUTF8String:download.filename.c_str()];
        if (!filename || filename.length == 0) {
            // Extract from URL if no filename
            NSString* url = [NSString stringWithUTF8String:download.url.c_str()];
            filename = [url lastPathComponent];
            if (!filename || filename.length == 0) {
                filename = @"download";
            }
        }

        NSTextField* filenameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
            [DSSpacing sm], rowHeight - 24, contentWidth - [DSSpacing xl] - 40, 18)];
        filenameLabel.stringValue = filename ?: @"Unknown";
        filenameLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
        filenameLabel.textColor = [DSColors textPrimary];
        filenameLabel.bezeled = NO;
        filenameLabel.drawsBackground = NO;
        filenameLabel.editable = NO;
        filenameLabel.selectable = NO;
        filenameLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
        filenameLabel.autoresizingMask = NSViewWidthSizable;
        [row addSubview:filenameLabel];

        // Status icon/button on right
        DSIconButton* actionBtn = nil;
        if (download.state == DownloadState::InProgress || download.state == DownloadState::Paused) {
            actionBtn = [DSIconButton buttonWithIcon:@"xmark.circle" tooltip:@"Cancel"];
        } else if (download.state == DownloadState::Complete && !fileMissing) {
            actionBtn = [DSIconButton buttonWithIcon:@"folder" tooltip:@"Show in Finder"];
        } else {
            // Stopped, failed, or file missing - show remove button
            actionBtn = [DSIconButton buttonWithIcon:@"xmark.circle" tooltip:@"Remove"];
        }

        if (actionBtn) {
            actionBtn.frame = NSMakeRect(contentWidth - [DSSpacing sm] - 28, rowHeight - 28, 24, 24);
            actionBtn.autoresizingMask = NSViewMinXMargin;  // Anchor to right edge
            [row addSubview:actionBtn];

            // Set action based on state
            if (download.state == DownloadState::InProgress || download.state == DownloadState::Paused) {
                actionBtn.target = self;
                actionBtn.action = @selector(cancelDownload:);
                objc_setAssociatedObject(actionBtn, "downloadId",
                    [NSNumber numberWithUnsignedInt:download.id], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            } else if (download.state == DownloadState::Complete && !fileMissing) {
                NSString* path = [NSString stringWithUTF8String:download.full_path.c_str()];
                actionBtn.target = self;
                actionBtn.action = @selector(revealDownload:);
                objc_setAssociatedObject(actionBtn, "downloadPath", path, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            } else {
                // Stopped, failed, or file missing - remove from list
                actionBtn.target = self;
                actionBtn.action = @selector(removeDownloadFromList:);
                objc_setAssociatedObject(actionBtn, "downloadId",
                    [NSNumber numberWithUnsignedInt:download.id], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
        }

        // Progress bar and status (only for in-progress downloads)
        if (download.state == DownloadState::InProgress || download.state == DownloadState::Paused) {
            BOOL sizeKnown = (download.total_bytes > 0 && download.percent_complete >= 0);

            // Only show progress bar when size is known
            if (sizeKnown) {
                // Progress background
                NSView* progressBg = [[NSView alloc] initWithFrame:NSMakeRect(
                    [DSSpacing sm], 28, contentWidth - [DSSpacing xl] - 8, 6)];
                progressBg.wantsLayer = YES;
                progressBg.layer.backgroundColor = [DSColors surface].CGColor;
                progressBg.layer.cornerRadius = 3;
                progressBg.autoresizingMask = NSViewWidthSizable;
                [row addSubview:progressBg];

                // Progress fill - use percentage of parent width
                CGFloat progressPercent = download.percent_complete / 100.0;
                CGFloat progressWidth = (contentWidth - [DSSpacing xl] - 8) * progressPercent;
                NSView* progressFill = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, progressWidth, 6)];
                progressFill.wantsLayer = YES;
                progressFill.layer.backgroundColor = download.state == DownloadState::Paused
                    ? [DSColors warning].CGColor : [DSColors accent].CGColor;
                progressFill.layer.cornerRadius = 3;
                progressFill.autoresizingMask = NSViewWidthSizable;
                [progressBg addSubview:progressFill];
            }

            // Status text: percentage, speed, size
            NSString* statusText;
            if (download.state == DownloadState::Paused) {
                if (sizeKnown) {
                    statusText = [NSString stringWithFormat:@"Paused - %d%% of %@",
                        download.percent_complete, [self formatBytes:download.total_bytes]];
                } else {
                    statusText = [NSString stringWithFormat:@"Paused - %@",
                        [self formatBytes:download.received_bytes]];
                }
            } else {
                // In progress
                if (sizeKnown) {
                    // Known total size
                    statusText = [NSString stringWithFormat:@"%d%% - %@ - %@ of %@",
                        download.percent_complete,
                        [self formatSpeed:download.current_speed],
                        [self formatBytes:download.received_bytes],
                        [self formatBytes:download.total_bytes]];
                } else {
                    // Unknown total size - just show downloaded amount and speed
                    statusText = [NSString stringWithFormat:@"%@ - %@",
                        [self formatBytes:download.received_bytes],
                        [self formatSpeed:download.current_speed]];
                }
            }

            // Adjust status label position based on whether progress bar is shown
            CGFloat statusY = sizeKnown ? 8 : 20;
            NSTextField* statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
                [DSSpacing sm], statusY, contentWidth - [DSSpacing xl], 16)];
            statusLabel.stringValue = statusText;
            statusLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
            statusLabel.textColor = [DSColors textSecondary];
            statusLabel.bezeled = NO;
            statusLabel.drawsBackground = NO;
            statusLabel.autoresizingMask = NSViewWidthSizable;
            statusLabel.editable = NO;
            statusLabel.selectable = NO;
            [row addSubview:statusLabel];
        } else {
            // Completed/stopped/failed status
            NSString* statusText;
            NSColor* statusColor;
            if (fileMissing) {
                // File was downloaded but deleted from disk
                statusText = @"File deleted - double-click to re-download";
                statusColor = [DSColors textSecondary];
                // Gray out the filename too
                filenameLabel.textColor = [DSColors textSecondary];
            } else if (download.state == DownloadState::Complete) {
                statusText = [NSString stringWithFormat:@"Completed - %@",
                    [self formatBytes:download.total_bytes > 0 ? download.total_bytes : download.received_bytes]];
                statusColor = [DSColors success];
            } else if (download.state == DownloadState::Canceled) {
                if (download.received_bytes > 0) {
                    statusText = [NSString stringWithFormat:@"Stopped - %@ downloaded",
                        [self formatBytes:download.received_bytes]];
                } else {
                    statusText = @"Stopped";
                }
                statusColor = [DSColors textSecondary];
            } else {
                statusText = @"Failed";
                statusColor = [DSColors error];
            }

            NSTextField* statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
                [DSSpacing sm], 20, contentWidth - [DSSpacing xl], 16)];
            statusLabel.stringValue = statusText;
            statusLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
            statusLabel.textColor = statusColor;
            statusLabel.bezeled = NO;
            statusLabel.drawsBackground = NO;
            statusLabel.editable = NO;
            statusLabel.selectable = NO;
            statusLabel.autoresizingMask = NSViewWidthSizable;
            [row addSubview:statusLabel];
        }

        [_downloadsContainer addSubview:row];
        y += rowHeight + [DSSpacing xs];
    }

    _downloadsContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _downloadsScrollView.bounds.size.height));
}

- (void)revealDownload:(DSIconButton*)sender {
    NSString* path = objc_getAssociatedObject(sender, "downloadPath");
    if (path) {
        [[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:@""];
    }
}

- (void)cancelDownload:(DSIconButton*)sender {
    NSNumber* downloadIdNum = objc_getAssociatedObject(sender, "downloadId");
    if (downloadIdNum) {
        uint32_t downloadId = [downloadIdNum unsignedIntValue];
        DownloadManager::GetInstance().CancelDownload(downloadId);
        [self reloadDownloads];
    }
}

- (void)removeDownloadFromList:(DSIconButton*)sender {
    NSNumber* downloadIdNum = objc_getAssociatedObject(sender, "downloadId");
    if (downloadIdNum) {
        uint32_t downloadId = [downloadIdNum unsignedIntValue];
        DownloadManager::GetInstance().RemoveDownload(downloadId);
        [self reloadDownloads];
    }
}

- (void)selectDownloadRow:(DownloadRowView*)row {
    // Deselect previous
    if (_selectedDownloadRow && _selectedDownloadRow != row) {
        _selectedDownloadRow.isSelected = NO;
        [_selectedDownloadRow setNeedsDisplay:YES];
    }

    // Select new
    _selectedDownloadRow = row;
    row.isSelected = YES;
    [row setNeedsDisplay:YES];
}

- (void)restartDownload:(NSString*)url {
    [self restartDownload:url removingDownloadId:0];
}

- (void)restartDownload:(NSString*)url removingDownloadId:(uint32_t)downloadId {
    (void)downloadId;  // Not removing anymore - new download will appear as separate entry
    if (!_windowController || !url) return;

    // Set pending original URL for the new download
    DownloadManager::GetInstance().SetPendingOriginalUrl([url UTF8String]);

    // Mark this as a restart so the saved download preference is used
    DownloadManager::GetInstance().SetIsRestart(true);

    // Navigate to the URL to restart the download
    [_windowController navigateToURL:url];
}

#pragma mark - Lifecycle

- (void)dealloc {
    // Clear the download manager callback to prevent it from calling back into a deallocated view
    DownloadManager::GetInstance().SetUpdateCallback(nullptr);
}

#pragma mark - Drawing

- (BOOL)isFlipped { return NO; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[DSColors background] setFill];
    NSRectFill(self.bounds);
}

@end
