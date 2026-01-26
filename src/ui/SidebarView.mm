#import "SidebarView.h"
#import "MainWindowController.h"
#import "Components.h"
#import <objc/runtime.h>
#include "history_storage.h"
#include "bookmark_storage.h"
#include "download_manager.h"

// Extern function to access global storage
extern HistoryStorage* GetHistoryStorage();
extern BookmarkStorage* GetBookmarkStorage();

// Pasteboard type for bookmark drag & drop
static NSString* const kBookmarkPasteboardType = @"com.orbfox.bookmark";

// Layout constants
static const CGFloat kIconStripWidth = 44.0;
static const CGFloat kSidebarWidth = 280.0;
static const CGFloat kWorkspaceHeight = 40.0;
static const CGFloat kNewTabButtonHeight = 44.0;

// ============================================================================
// FAVICON CACHE
// Caches favicons by domain for use in bookmarks/history
// ============================================================================

static NSMutableDictionary<NSString*, NSImage*>* sFaviconCache = nil;

static NSString* GetDomainFromURL(NSString* urlString) {
    if (!urlString || urlString.length == 0) return nil;
    NSURL* url = [NSURL URLWithString:urlString];
    return url.host;
}

static NSImage* GetCachedFavicon(NSString* urlString) {
    if (!sFaviconCache) return nil;
    NSString* domain = GetDomainFromURL(urlString);
    if (!domain) return nil;
    return sFaviconCache[domain];
}

static void CacheFavicon(NSString* urlString, NSImage* favicon) {
    if (!favicon || !urlString) return;
    if (!sFaviconCache) {
        sFaviconCache = [NSMutableDictionary dictionary];
    }
    NSString* domain = GetDomainFromURL(urlString);
    if (domain) {
        sFaviconCache[domain] = favicon;
    }
}

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
// BOOKMARK DROP CONTAINER
// FlippedView that accepts bookmark drops for reordering and moving to folders
// ============================================================================

@class SidebarView;

@interface BookmarkDropContainerView : NSView <NSDraggingDestination>
@property (nonatomic, weak) SidebarView* sidebarView;
@property (nonatomic, strong) NSView* dropIndicator;
@property (nonatomic, strong) NSView* highlightedFolderHeader;
@property (nonatomic, copy) NSString* dropTargetFolder;   // nil = root level
@property (nonatomic, assign) int dropPosition;           // Position within folder
@property (nonatomic, assign) BOOL dropOnFolderHeader;    // YES if dropping ON folder (not between items)
@end

@implementation BookmarkDropContainerView

- (BOOL)isFlipped { return YES; }

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[kBookmarkPasteboardType]];

        // Create drop indicator (2px accent-colored line)
        _dropIndicator = [[NSView alloc] initWithFrame:NSZeroRect];
        _dropIndicator.wantsLayer = YES;
        _dropIndicator.layer.backgroundColor = [DSColors accent].CGColor;
        _dropIndicator.layer.cornerRadius = 1;
        _dropIndicator.hidden = YES;
        [self addSubview:_dropIndicator];

        _dropPosition = 0;
    }
    return self;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    (void)sender;
    return NSDragOperationMove;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint location = [self convertPoint:[sender draggingLocation] fromView:nil];

    // Reset highlight
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropOnFolderHeader = NO;
    _dropTargetFolder = nil;
    _dropPosition = 0;

    CGFloat indicatorY = 0;
    NSString* currentFolder = nil;  // Tracks current folder context
    int positionInFolder = 0;       // Position counter within current folder
    BOOL foundDropPoint = NO;

    // Sort subviews by Y position for proper iteration
    NSArray* sortedSubviews = [self.subviews sortedArrayUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        if (a == self->_dropIndicator) return NSOrderedDescending;
        if (b == self->_dropIndicator) return NSOrderedAscending;
        return [@(a.frame.origin.y) compare:@(b.frame.origin.y)];
    }];

    NSView* lastView = nil;
    for (NSView* subview in sortedSubviews) {
        if (subview == _dropIndicator) continue;

        NSRect frame = subview.frame;

        // Check if this is a folder header (not a DSRow and height < 40)
        BOOL isFolderHeader = (![subview isKindOfClass:[DSRow class]] && frame.size.height < 40);

        if (isFolderHeader) {
            // Get folder name from header
            NSString* folderName = nil;
            for (NSView* headerSubview in subview.subviews) {
                if ([headerSubview isKindOfClass:[NSTextField class]]) {
                    NSTextField* label = (NSTextField*)headerSubview;
                    if (label.frame.origin.x > 30) {  // Folder name label (not chevron area)
                        folderName = label.stringValue;
                        break;
                    }
                }
            }

            // Check if dropping ON this folder header
            if (location.y >= frame.origin.y && location.y < frame.origin.y + frame.size.height) {
                // Highlight the folder header
                subview.wantsLayer = YES;
                subview.layer.backgroundColor = [DSColors surfaceHover].CGColor;
                _highlightedFolderHeader = subview;
                _dropOnFolderHeader = YES;
                _dropTargetFolder = folderName;
                _dropPosition = 0;  // First position in folder
                _dropIndicator.hidden = YES;
                foundDropPoint = YES;
                break;
            }

            // If mouse is before this folder header, drop at end of previous section
            if (location.y < frame.origin.y && !foundDropPoint) {
                _dropTargetFolder = currentFolder;
                _dropPosition = positionInFolder;
                indicatorY = frame.origin.y - 1;
                foundDropPoint = YES;
                break;
            }

            // Update current folder context
            currentFolder = folderName;
            positionInFolder = 0;

        } else if ([subview isKindOfClass:[DSRow class]]) {
            // This is a bookmark row
            CGFloat midY = frame.origin.y + frame.size.height / 2;

            if (location.y < midY && !foundDropPoint) {
                // Drop before this bookmark
                _dropTargetFolder = currentFolder;
                _dropPosition = positionInFolder;
                indicatorY = frame.origin.y - 1;
                foundDropPoint = YES;
                break;
            }

            positionInFolder++;
        }

        lastView = subview;
    }

    // If past all items, drop at end
    if (!foundDropPoint && lastView) {
        _dropTargetFolder = currentFolder;
        _dropPosition = positionInFolder;
        indicatorY = lastView.frame.origin.y + lastView.frame.size.height + 1;
    }

    // Show drop indicator (unless dropping ON a folder)
    if (!_dropOnFolderHeader) {
        CGFloat padding = [DSSpacing xs];
        CGFloat indent = (_dropTargetFolder.length > 0) ? [DSSpacing md] : 0;
        _dropIndicator.frame = NSMakeRect(padding + indent, indicatorY, self.bounds.size.width - padding * 2 - indent, 2);
        _dropIndicator.hidden = NO;
    }

    return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropTargetFolder = nil;
    _dropPosition = 0;
    _dropOnFolderHeader = NO;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }

    NSPasteboard* pboard = [sender draggingPasteboard];
    NSData* data = [pboard dataForType:kBookmarkPasteboardType];
    if (!data) return NO;

    NSDictionary* dragData = [NSPropertyListSerialization propertyListWithData:data
                                                                       options:NSPropertyListImmutable
                                                                        format:nil
                                                                         error:nil];
    if (!dragData) return NO;

    int64_t bookmarkId = [dragData[@"id"] longLongValue];
    NSString* sourceFolder = dragData[@"folder"];
    (void)sourceFolder;  // Currently unused, but available for future use

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return NO;

    // Determine target folder
    NSString* targetFolder = _dropTargetFolder ?: @"";

    // Adjust position if moving within same folder (need to account for removed item)
    int finalPosition = _dropPosition;
    if ([sourceFolder isEqualToString:targetFolder] || (sourceFolder.length == 0 && targetFolder.length == 0)) {
        // Moving within same folder - check if source is before drop position
        std::vector<Bookmark> folderBookmarks;
        if (targetFolder.length > 0) {
            folderBookmarks = bookmarks->GetBookmarksInFolder([targetFolder UTF8String]);
        } else {
            std::vector<Bookmark> all = bookmarks->GetAllBookmarks();
            for (const auto& bm : all) {
                if (bm.folder.empty()) {
                    folderBookmarks.push_back(bm);
                }
            }
        }

        int sourcePosition = -1;
        for (size_t i = 0; i < folderBookmarks.size(); i++) {
            if (folderBookmarks[i].id == bookmarkId) {
                sourcePosition = (int)i;
                break;
            }
        }

        if (sourcePosition >= 0 && sourcePosition < finalPosition) {
            finalPosition--;  // Account for removal
        }
    }

    // Move the bookmark
    bookmarks->MoveBookmark(bookmarkId, [targetFolder UTF8String], finalPosition);

    // Reload bookmarks in sidebar
    if (_sidebarView) {
        [_sidebarView reloadBookmarks];
    }

    return YES;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropTargetFolder = nil;
    _dropPosition = 0;
    _dropOnFolderHeader = NO;
}

@end

// ============================================================================
// ADD BOOKMARK POPOVER
// Popover for adding a new bookmark with URL, title, nickname, description
// ============================================================================

@class SidebarView;

@interface AddBookmarkPopoverController : NSViewController
@property (nonatomic, weak) SidebarView* sidebarView;
- (void)showRelativeToView:(NSView*)view withUrl:(NSString*)url title:(NSString*)title;
- (void)close;
@end

@implementation AddBookmarkPopoverController {
    NSPopover* _popover;
    NSTextField* _titleField;
    NSTextField* _urlField;
    NSTextField* _nicknameField;
    NSTextView* _descriptionField;
    NSPopUpButton* _folderPicker;
    NSImageView* _bookmarkIcon;
}

- (void)loadView {
    CGFloat width = 340;
    CGFloat height = 280;

    NSView* contentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    contentView.wantsLayer = YES;

    CGFloat padding = [DSSpacing md];
    CGFloat labelHeight = 16;
    CGFloat fieldHeight = 28;
    CGFloat y = height - padding;

    // Title label
    y -= labelHeight;
    NSTextField* titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, 60, labelHeight)];
    titleLabel.stringValue = @"Title";
    titleLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    titleLabel.textColor = [DSColors textSecondary];
    titleLabel.bezeled = NO;
    titleLabel.drawsBackground = NO;
    titleLabel.editable = NO;
    [contentView addSubview:titleLabel];

    // Bookmark icon (right side)
    CGFloat iconSize = 60;
    _bookmarkIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(width - padding - iconSize, y - 50, iconSize, iconSize)];
    _bookmarkIcon.image = [NSImage imageWithSystemSymbolName:@"bookmark.fill" accessibilityDescription:nil];
    _bookmarkIcon.contentTintColor = [DSColors textSecondary];
    _bookmarkIcon.wantsLayer = YES;
    _bookmarkIcon.layer.backgroundColor = [DSColors surface].CGColor;
    _bookmarkIcon.layer.cornerRadius = [DSLayout cornerRadiusMedium];
    [contentView addSubview:_bookmarkIcon];

    // Title field (Address - URL input)
    y -= fieldHeight + 4;
    CGFloat fieldWidth = width - padding * 2 - iconSize - [DSSpacing sm];
    _urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, fieldWidth, fieldHeight)];
    _urlField.font = [DSTypography fontWithStyle:DSFontStyleBody];;
    _urlField.bezelStyle = NSTextFieldRoundedBezel;
    _urlField.placeholderString = @"Address";
    [contentView addSubview:_urlField];

    // Nickname label
    y -= labelHeight + [DSSpacing sm];
    NSTextField* nicknameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, 80, labelHeight)];
    nicknameLabel.stringValue = @"Nickname";
    nicknameLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    nicknameLabel.textColor = [DSColors textSecondary];
    nicknameLabel.bezeled = NO;
    nicknameLabel.drawsBackground = NO;
    nicknameLabel.editable = NO;
    [contentView addSubview:nicknameLabel];

    // Nickname field
    y -= fieldHeight + 4;
    _titleField = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, fieldHeight)];
    _titleField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _titleField.bezelStyle = NSTextFieldRoundedBezel;
    _titleField.placeholderString = @"Bookmark name";
    [contentView addSubview:_titleField];

    // Description label
    y -= labelHeight + [DSSpacing sm];
    NSTextField* descLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, 80, labelHeight)];
    descLabel.stringValue = @"Description";
    descLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    descLabel.textColor = [DSColors textSecondary];
    descLabel.bezeled = NO;
    descLabel.drawsBackground = NO;
    descLabel.editable = NO;
    [contentView addSubview:descLabel];

    // Description text view (multiline)
    y -= 60 + 4;
    NSScrollView* descScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 60)];
    descScrollView.hasVerticalScroller = YES;
    descScrollView.hasHorizontalScroller = NO;
    descScrollView.borderType = NSBezelBorder;

    _descriptionField = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, width - padding * 2 - 4, 56)];
    _descriptionField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _descriptionField.textColor = [DSColors textPrimary];
    _descriptionField.backgroundColor = [DSColors surface];
    _descriptionField.minSize = NSMakeSize(0, 56);
    _descriptionField.maxSize = NSMakeSize(FLT_MAX, FLT_MAX);
    _descriptionField.verticallyResizable = YES;
    _descriptionField.horizontallyResizable = NO;
    _descriptionField.textContainer.widthTracksTextView = YES;
    descScrollView.documentView = _descriptionField;
    [contentView addSubview:descScrollView];

    // Folder label and picker
    y -= fieldHeight + [DSSpacing md];
    NSTextField* folderLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y + 4, 50, labelHeight)];
    folderLabel.stringValue = @"Folder";
    folderLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    folderLabel.textColor = [DSColors textSecondary];
    folderLabel.bezeled = NO;
    folderLabel.drawsBackground = NO;
    folderLabel.editable = NO;
    [contentView addSubview:folderLabel];

    _folderPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(padding + 55, y, width - padding * 2 - 55, 26) pullsDown:NO];
    [self populateFolderPicker];
    [contentView addSubview:_folderPicker];

    // Buttons
    y -= 40;
    NSButton* cancelBtn = [[NSButton alloc] initWithFrame:NSMakeRect(padding, y, 70, 28)];
    cancelBtn.title = @"Cancel";
    cancelBtn.bezelStyle = NSBezelStyleRounded;
    cancelBtn.target = self;
    cancelBtn.action = @selector(cancelClicked:);
    [contentView addSubview:cancelBtn];

    NSButton* addBtn = [[NSButton alloc] initWithFrame:NSMakeRect(width - padding - 70, y, 70, 28)];
    addBtn.title = @"Add";
    addBtn.bezelStyle = NSBezelStyleRounded;
    addBtn.keyEquivalent = @"\r";
    addBtn.target = self;
    addBtn.action = @selector(addClicked:);
    [contentView addSubview:addBtn];

    self.view = contentView;
}

- (void)populateFolderPicker {
    [_folderPicker removeAllItems];
    [_folderPicker addItemWithTitle:@"No Folder"];

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    std::vector<std::string> folders = bookmarks->GetFolders();
    if (!folders.empty()) {
        [[_folderPicker menu] addItem:[NSMenuItem separatorItem]];
        for (const auto& folder : folders) {
            [_folderPicker addItemWithTitle:[NSString stringWithUTF8String:folder.c_str()]];
        }
    }
}

- (void)showRelativeToView:(NSView*)view withUrl:(NSString*)url title:(NSString*)title {
    if (!self.view) {
        [self loadView];
    }

    // Set default values
    _urlField.stringValue = url ?: @"";
    _titleField.stringValue = title ?: @"";
    _descriptionField.string = @"";

    [self populateFolderPicker];
    [_folderPicker selectItemAtIndex:0];

    if (!_popover) {
        _popover = [[NSPopover alloc] init];
        _popover.contentViewController = self;
        _popover.behavior = NSPopoverBehaviorTransient;
    }

    [_popover showRelativeToRect:view.bounds
                          ofView:view
                   preferredEdge:NSRectEdgeMinY];

    // Focus the URL field
    [_popover.contentViewController.view.window makeFirstResponder:_urlField];
}

- (void)close {
    [_popover close];
}

- (void)cancelClicked:(id)sender {
    (void)sender;
    [_popover close];
}

- (void)addClicked:(id)sender {
    (void)sender;

    NSString* url = _urlField.stringValue;
    NSString* title = _titleField.stringValue;

    // Validate URL
    if (url.length == 0) {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"URL Required";
        alert.informativeText = @"Please enter a URL for the bookmark.";
        alert.alertStyle = NSAlertStyleWarning;
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }

    // Add http:// if no scheme
    if (![url containsString:@"://"]) {
        url = [@"https://" stringByAppendingString:url];
    }

    // Use URL as title if title is empty
    if (title.length == 0) {
        title = url;
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        NSString* folder = @"";
        NSInteger selectedIndex = [_folderPicker indexOfSelectedItem];
        if (selectedIndex > 0) {
            NSMenuItem* selectedItem = [_folderPicker selectedItem];
            if (!selectedItem.isSeparatorItem) {
                folder = selectedItem.title;
            }
        }

        bookmarks->AddBookmark([url UTF8String], [title UTF8String], [folder UTF8String]);

        if (_sidebarView) {
            [_sidebarView reloadBookmarks];
        }
    }

    [_popover close];
}

@end

// ============================================================================
// ROUNDED RECT PROGRESS VIEW
// Rounded rectangle progress ring for downloads icon (matches icon button shape)
// ============================================================================

@interface CircularProgressView : NSView
@property (nonatomic, assign) CGFloat progress;  // 0.0 to 1.0
@property (nonatomic, strong) NSColor* trackColor;
@property (nonatomic, strong) NSColor* progressColor;
@property (nonatomic, assign) CGFloat lineWidth;
@property (nonatomic, assign) CGFloat cornerRadius;
@end

@implementation CircularProgressView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _progress = 0.0;
        _trackColor = [[DSColors textSecondary] colorWithAlphaComponent:0.3];
        _progressColor = [DSColors accent];
        _lineWidth = 2.5;
        _cornerRadius = [DSLayout cornerRadiusMedium];
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
    return self;
}

- (BOOL)isOpaque {
    return NO;
}

- (void)setProgress:(CGFloat)progress {
    _progress = progress;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    NSRect insetRect = NSInsetRect(self.bounds, _lineWidth / 2.0, _lineWidth / 2.0);
    CGFloat cr = MIN(_cornerRadius, MIN(insetRect.size.width, insetRect.size.height) / 2.0);

    // Draw track (background rounded rect)
    NSBezierPath* trackPath = [NSBezierPath bezierPathWithRoundedRect:insetRect
                                                              xRadius:cr
                                                              yRadius:cr];
    [_trackColor setStroke];
    trackPath.lineWidth = _lineWidth;
    [trackPath stroke];

    // Draw progress starting from top-center, going clockwise
    if (_progress > 0.001) {
        CGFloat x = NSMinX(insetRect);
        CGFloat y = NSMinY(insetRect);
        CGFloat w = NSWidth(insetRect);
        CGFloat h = NSHeight(insetRect);

        // Calculate perimeter segments (clockwise from top-center)
        // Segments: top-right half, top-right corner arc, right side, bottom-right corner arc,
        //           bottom side, bottom-left corner arc, left side, top-left corner arc, top-left half
        CGFloat arcLength = cr * M_PI / 2.0;  // Quarter circle arc length
        CGFloat topHalf = (w - 2 * cr) / 2.0;
        CGFloat rightSide = h - 2 * cr;
        CGFloat bottomSide = w - 2 * cr;
        CGFloat leftSide = h - 2 * cr;

        CGFloat totalPerimeter = 2 * topHalf + 4 * arcLength + rightSide + bottomSide + leftSide;
        CGFloat targetLength = _progress * totalPerimeter;

        NSBezierPath* progressPath = [NSBezierPath bezierPath];
        progressPath.lineWidth = _lineWidth;
        progressPath.lineCapStyle = NSLineCapStyleRound;

        CGFloat midTopX = x + w / 2.0;
        CGFloat topY = y + h;
        [progressPath moveToPoint:NSMakePoint(midTopX, topY)];

        CGFloat drawn = 0;

        // Segment 1: Top-right half line
        if (drawn < targetLength) {
            CGFloat segLen = topHalf;
            CGFloat endX = x + w - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(endX, topY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(midTopX + (topHalf * ratio), topY)];
                drawn = targetLength;
            }
        }

        // Segment 2: Top-right corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + w - cr;
            CGFloat arcCenterY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:90
                                                       endAngle:0
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = 90 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:90
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 3: Right side
        if (drawn < targetLength) {
            CGFloat segLen = rightSide;
            CGFloat endY = y + cr;
            CGFloat rightX = x + w;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(rightX, endY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(rightX, (y + h - cr) - rightSide * ratio)];
                drawn = targetLength;
            }
        }

        // Segment 4: Bottom-right corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + w - cr;
            CGFloat arcCenterY = y + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:0
                                                       endAngle:-90
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = -90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:0
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 5: Bottom side
        if (drawn < targetLength) {
            CGFloat segLen = bottomSide;
            CGFloat endX = x + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(endX, y)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint((x + w - cr) - bottomSide * ratio, y)];
                drawn = targetLength;
            }
        }

        // Segment 6: Bottom-left corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + cr;
            CGFloat arcCenterY = y + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:-90
                                                       endAngle:-180
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = -90 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:-90
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 7: Left side
        if (drawn < targetLength) {
            CGFloat segLen = leftSide;
            CGFloat endY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(x, endY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(x, (y + cr) + leftSide * ratio)];
                drawn = targetLength;
            }
        }

        // Segment 8: Top-left corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + cr;
            CGFloat arcCenterY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:180
                                                       endAngle:90
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = 180 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:180
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 9: Top-left half line
        if (drawn < targetLength) {
            CGFloat segLen = topHalf;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(midTopX, topY)];
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint((x + cr) + topHalf * ratio, topY)];
            }
        }

        [_progressColor setStroke];
        [progressPath stroke];
    }
}

@end

// ============================================================================
// DOWNLOAD ROW VIEW
// Row view with right-click context menu, selection, and double-click for downloads
// ============================================================================

@interface DownloadRowView : NSView
@property (nonatomic, assign) uint32_t downloadId;
@property (nonatomic, copy) NSString* downloadPath;
@property (nonatomic, copy) NSString* downloadUrl;
@property (nonatomic, assign) BOOL isInProgress;
@property (nonatomic, assign) BOOL isStopped;
@property (nonatomic, assign) BOOL isComplete;
@property (nonatomic, assign) BOOL isFileMissing;  // File was downloaded but deleted from disk
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, weak) SidebarView* sidebarView;
@end

@implementation DownloadRowView {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _isSelected = NO;
        _isHovered = NO;
    }
    return self;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                 options:(NSTrackingMouseEnteredAndExited |
                                                         NSTrackingActiveInKeyWindow)
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    _isHovered = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    // Select this row
    [_sidebarView selectDownloadRow:self];
    [super mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event {
    if (event.clickCount == 2) {
        [self handleDoubleClick];
    }
    [super mouseUp:event];
}

- (void)handleDoubleClick {
    if (_isFileMissing) {
        // File was deleted - restart download with previous choice
        if (_downloadUrl) {
            [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
        }
    } else if (_isComplete) {
        // Open completed download
        if (_downloadPath) {
            [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:_downloadPath]];
        }
    } else if (_isStopped) {
        // Restart stopped download - open URL in browser
        if (_downloadUrl) {
            [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
        }
    }
    // For in-progress downloads, double-click does nothing
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Draw selection/hover background
    if (_isSelected) {
        [[DSColors accent] colorWithAlphaComponent:0.2].set;
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                            xRadius:self.layer.cornerRadius
                                                            yRadius:self.layer.cornerRadius];
        [path fill];
    } else if (_isHovered) {
        [[DSColors surface] colorWithAlphaComponent:0.5].set;
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                            xRadius:self.layer.cornerRadius
                                                            yRadius:self.layer.cornerRadius];
        [path fill];
    }
}

- (NSMenu*)menuForEvent:(NSEvent*)event {
    // Select the row when right-clicking
    [_sidebarView selectDownloadRow:self];

    NSMenu* menu = [[NSMenu alloc] init];

    if (_isInProgress) {
        // In-progress download menu
        NSMenuItem* stopItem = [[NSMenuItem alloc] initWithTitle:@"Stop"
                                                          action:@selector(stopDownload:)
                                                   keyEquivalent:@""];
        stopItem.target = self;
        [menu addItem:stopItem];
    } else if (_isComplete) {
        // Completed download menu
        NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                          action:@selector(openDownload:)
                                                   keyEquivalent:@""];
        openItem.target = self;
        [menu addItem:openItem];

        NSMenuItem* showItem = [[NSMenuItem alloc] initWithTitle:@"Show in Finder"
                                                          action:@selector(showInFinder:)
                                                   keyEquivalent:@""];
        showItem.target = self;
        [menu addItem:showItem];
    } else if (_isStopped) {
        // Stopped download menu
        NSMenuItem* restartItem = [[NSMenuItem alloc] initWithTitle:@"Restart Download"
                                                             action:@selector(restartDownloadAction:)
                                                      keyEquivalent:@""];
        restartItem.target = self;
        [menu addItem:restartItem];
    }

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* copyUrlItem = [[NSMenuItem alloc] initWithTitle:@"Copy Download Address"
                                                         action:@selector(copyDownloadUrl:)
                                                  keyEquivalent:@""];
    copyUrlItem.target = self;
    [menu addItem:copyUrlItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* removeItem = [[NSMenuItem alloc] initWithTitle:@"Remove from List"
                                                        action:@selector(removeDownload:)
                                                 keyEquivalent:@""];
    removeItem.target = self;
    [menu addItem:removeItem];

    return menu;
}

- (void)openDownload:(id)sender {
    (void)sender;
    if (_downloadPath) {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:_downloadPath]];
    }
}

- (void)showInFinder:(id)sender {
    (void)sender;
    if (_downloadPath) {
        [[NSWorkspace sharedWorkspace] selectFile:_downloadPath inFileViewerRootedAtPath:@""];
    }
}

- (void)stopDownload:(id)sender {
    (void)sender;
    // Cancel the download through the manager (which calls CEF cancel callback)
    DownloadManager::GetInstance().CancelDownload(_downloadId);
    [_sidebarView reloadDownloads];
}

- (void)restartDownloadAction:(id)sender {
    (void)sender;
    if (_downloadUrl) {
        [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
    }
}

- (void)copyDownloadUrl:(id)sender {
    (void)sender;
    if (_downloadUrl) {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:_downloadUrl forType:NSPasteboardTypeString];
    }
}

- (void)removeDownload:(id)sender {
    (void)sender;
    // Just remove from list, don't cancel if in progress
    DownloadManager::GetInstance().RemoveDownload(_downloadId);
    [_sidebarView reloadDownloads];
}

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

    // Rename Tab
    NSMenuItem* renameItem = [[NSMenuItem alloc] initWithTitle:@"Rename Tab"
                                                        action:@selector(renameTab:)
                                                 keyEquivalent:@""];
    renameItem.target = self;
    [menu addItem:renameItem];

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

    // Open in Space submenu
    NSMenuItem* openInSpaceItem = [[NSMenuItem alloc] initWithTitle:@"Open in Space"
                                                             action:nil
                                                      keyEquivalent:@""];
    NSMenu* spaceSubmenu = [[NSMenu alloc] initWithTitle:@"Spaces"];

    TabManager* tabManager = _sidebarView.windowController.tabManager;
    if (tabManager) {
        for (const auto& workspace : tabManager->GetWorkspaces()) {
            NSMenuItem* wsItem = [[NSMenuItem alloc] initWithTitle:
                [NSString stringWithUTF8String:workspace->name.c_str()]
                                                            action:@selector(duplicateTabToWorkspace:)
                                                     keyEquivalent:@""];
            wsItem.target = self;
            wsItem.tag = workspace->id;
            [spaceSubmenu addItem:wsItem];
        }
    }

    [spaceSubmenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* newSpaceItem = [[NSMenuItem alloc] initWithTitle:@"New Space"
                                                          action:@selector(duplicateTabToNewWorkspace:)
                                                   keyEquivalent:@""];
    newSpaceItem.target = self;
    [spaceSubmenu addItem:newSpaceItem];

    openInSpaceItem.submenu = spaceSubmenu;
    [menu addItem:openInSpaceItem];

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

- (void)renameTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!tab) return;

    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Rename Tab";
    alert.informativeText = @"Enter a new title for this tab:";
    [alert addButtonWithTitle:@"Rename"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 250, 24)];
    input.stringValue = [NSString stringWithUTF8String:tab->title.c_str()];
    alert.accessoryView = input;

    __weak TabRowView* weakSelf = self;
    int tabId = _tabId;
    [alert beginSheetModalForWindow:_sidebarView.windowController.window
                  completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            NSString* newTitle = input.stringValue;
            if (newTitle.length > 0) {
                TabRowView* strongSelf = weakSelf;
                if (strongSelf) {
                    Tab* t = strongSelf->_sidebarView.windowController.tabManager->GetTabById(tabId);
                    if (t) {
                        t->title = [newTitle UTF8String];
                        [strongSelf setTitle:newTitle];
                    }
                }
            }
        }
    }];
}

- (void)duplicateTabToWorkspace:(NSMenuItem*)sender {
    int targetWorkspaceId = (int)sender.tag;
    Tab* sourceTab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!sourceTab) return;

    NSString* urlStr = [NSString stringWithUTF8String:sourceTab->url.c_str()];

    // Switch to target workspace
    _sidebarView.windowController.tabManager->SetActiveWorkspace(targetWorkspaceId);

    // Create new tab with same URL
    [_sidebarView.windowController createNewTab:urlStr];

    // Reload UI
    [_sidebarView reloadWorkspaceTabs];
    [_sidebarView reloadTabs];
}

- (void)duplicateTabToNewWorkspace:(id)sender {
    (void)sender;
    Tab* sourceTab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!sourceTab) return;

    NSString* urlStr = [NSString stringWithUTF8String:sourceTab->url.c_str()];

    // Create new workspace
    TabManager* tabManager = _sidebarView.windowController.tabManager;
    size_t count = tabManager->GetWorkspaces().size();
    NSString* spaceName = [NSString stringWithFormat:@"WS %zu", count + 1];
    Workspace* newWorkspace = tabManager->CreateWorkspace([spaceName UTF8String]);

    if (!newWorkspace) return;

    // Switch to new workspace and create tab
    tabManager->SetActiveWorkspace(newWorkspace->id);
    [_sidebarView.windowController createNewTab:urlStr];

    // Reload UI
    [_sidebarView reloadWorkspaceTabs];
    [_sidebarView reloadTabs];
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
    NSMutableArray<NSView*>* _workspaceTabs;  // Array of workspace tab containers

    // History panel
    NSView* _historyPanelContainer;
    NSScrollView* _historyScrollView;
    FlippedView* _historyContainer;

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
                // Cache favicon by domain for use in bookmarks/history
                NSString* url = [NSString stringWithUTF8String:tab->url.c_str()];
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
}

- (void)updateTab:(int)tabId faviconData:(NSData*)faviconData {
    if (!faviconData || faviconData.length == 0) return;

    NSImage* favicon = [[NSImage alloc] initWithData:faviconData];
    if (!favicon) return;

    // Cache by URL for bookmarks/history
    if (_windowController) {
        Tab* tab = _windowController.tabManager->GetTabById(tabId);
        if (tab) {
            NSString* url = [NSString stringWithUTF8String:tab->url.c_str()];
            CacheFavicon(url, favicon);
        }
    }

    for (TabRowView* row in _tabRows) {
        if (row.tabId == tabId) {
            row.favicon = favicon;
            break;
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

    NSString* title = [NSString stringWithUTF8String:entry.title.empty()
        ? entry.url.c_str() : entry.title.c_str()];
    NSString* urlStr = [NSString stringWithUTF8String:entry.url.c_str()];
    NSString* folderStr = [NSString stringWithUTF8String:entry.folder.c_str()];

    row.title = title;
    row.showsCloseButton = YES;

    // Set favicon from cache if available
    NSImage* favicon = GetCachedFavicon(urlStr);
    if (favicon) {
        row.icon = favicon;
    }

    __weak SidebarView* weakSelf = self;
    NSString* urlCopy = urlStr;
    NSString* titleCopy = title;
    NSString* folderCopy = folderStr;
    int64_t bookmarkId = entry.id;

    // Associate bookmark ID with row for later lookup
    objc_setAssociatedObject(row, "bookmarkId", @(bookmarkId), OBJC_ASSOCIATION_RETAIN);

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
    if (url && _windowController) {
        [_windowController navigateToURL:url];
    }
}

- (void)openBookmarkInNewTab:(NSMenuItem*)sender {
    NSString* url = objc_getAssociatedObject(sender, "url");
    if (url && _windowController) {
        [_windowController createNewTab:url];
    }
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

- (NSView*)createFolderHeader:(NSString*)folderName
                          atY:(CGFloat)y
                        width:(CGFloat)width
                  isCollapsed:(BOOL)isCollapsed {
    CGFloat headerHeight = 36;
    NSView* header = [[NSView alloc] initWithFrame:NSMakeRect(0, y, width, headerHeight)];
    header.wantsLayer = YES;
    header.layer.cornerRadius = [DSLayout cornerRadiusMedium];

    CGFloat padding = [DSSpacing sm];
    CGFloat verticalCenter = (headerHeight - 16) / 2;  // Center 16px icons vertically

    // Chevron icon (expand/collapse indicator)
    NSString* chevronName = isCollapsed ? @"chevron.right" : @"chevron.down";
    NSImageView* chevron = [[NSImageView alloc] initWithFrame:NSMakeRect(padding, verticalCenter, 14, 14)];
    chevron.image = [NSImage imageWithSystemSymbolName:chevronName accessibilityDescription:nil];
    chevron.contentTintColor = [DSColors textSecondary];
    [header addSubview:chevron];

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
    [header addSubview:nameLabel];

    // "Open All" button (right side, hidden by default, shown on hover)
    DSIconButton* openAllBtn = [DSIconButton buttonWithIcon:@"arrow.up.right.square" tooltip:@"Open all in new tabs"];
    openAllBtn.frame = NSMakeRect(width - padding - 24, (headerHeight - 20) / 2, 20, 20);
    openAllBtn.target = self;
    openAllBtn.action = @selector(openAllInFolder:);
    openAllBtn.hidden = YES;  // Hidden by default
    objc_setAssociatedObject(openAllBtn, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    objc_setAssociatedObject(header, "openAllButton", openAllBtn, OBJC_ASSOCIATION_RETAIN);
    [header addSubview:openAllBtn];

    // Tracking area for hover
    NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
        initWithRect:header.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow)
               owner:self
            userInfo:@{@"header": header, @"openAllBtn": openAllBtn}];
    [header addTrackingArea:trackingArea];

    // Click area for expand/collapse (covers left part, not the button)
    NSButton* clickArea = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, width - 40, headerHeight)];
    clickArea.transparent = YES;
    clickArea.bordered = NO;
    clickArea.target = self;
    clickArea.action = @selector(toggleFolderCollapse:);
    objc_setAssociatedObject(clickArea, "folderName", folderName, OBJC_ASSOCIATION_RETAIN);
    [header addSubview:clickArea positioned:NSWindowBelow relativeTo:nil];

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

- (void)toggleFolderCollapse:(NSButton*)sender {
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
        row.onClick = ^{
            SidebarView* strongSelf = weakSelf;
            if (!strongSelf) return;
            // Open as new tab in active workspace
            [strongSelf.windowController createNewTab:urlCopy];
            strongSelf->_activePanel = SidebarPanelTabs;
            [strongSelf updateIconSelection];
            [strongSelf updatePanelVisibility];
        };

        [_historyContainer addSubview:row];
        y += rowHeight;
    }

    _historyContainer.frame = NSMakeRect(0, 0, contentWidth, MAX(y, _historyScrollView.bounds.size.height));
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

#pragma mark - Folder Header Hover

- (void)mouseEntered:(NSEvent*)event {
    NSDictionary* userInfo = event.trackingArea.userInfo;
    if (userInfo[@"openAllBtn"]) {
        DSIconButton* openAllBtn = userInfo[@"openAllBtn"];
        NSView* header = userInfo[@"header"];
        openAllBtn.hidden = NO;
        header.layer.backgroundColor = [DSColors surfaceHover].CGColor;
    }
}

- (void)mouseExited:(NSEvent*)event {
    NSDictionary* userInfo = event.trackingArea.userInfo;
    if (userInfo[@"openAllBtn"]) {
        DSIconButton* openAllBtn = userInfo[@"openAllBtn"];
        NSView* header = userInfo[@"header"];
        openAllBtn.hidden = YES;
        header.layer.backgroundColor = nil;
    }
}

#pragma mark - Drawing

- (BOOL)isFlipped { return NO; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[DSColors background] setFill];
    NSRectFill(self.bounds);
}

@end
