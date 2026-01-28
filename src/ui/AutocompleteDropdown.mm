#import "AutocompleteDropdown.h"
#import "MainWindowController.h"
#import "components/Components.h"
#include "history_storage.h"
#include "bookmark_storage.h"

extern HistoryStorage* GetHistoryStorage();
extern BookmarkStorage* GetBookmarkStorage();
extern NSImage* GetCachedFavicon(NSString* urlString);

// ============================================================================
// AUTOCOMPLETE SUGGESTION
// ============================================================================

@implementation AutocompleteSuggestion
@end

// ============================================================================
// AUTOCOMPLETE ROW VIEW
// ============================================================================

@interface AutocompleteRowView : NSView
@property (nonatomic, strong) AutocompleteSuggestion* suggestion;
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, copy) void (^onClick)(void);
@end

@implementation AutocompleteRowView {
    NSImageView* _faviconView;
    NSTextField* _titleLabel;
    NSTextField* _urlLabel;
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        [self setupSubviews];
    }
    return self;
}

- (void)setupSubviews {
    CGFloat padding = [DSSpacing sm];
    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat iconY = (self.bounds.size.height - iconSize) / 2;

    // Favicon
    _faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(padding, iconY, iconSize, iconSize)];
    _faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
    [self addSubview:_faviconView];

    // Title label
    CGFloat textX = padding + iconSize + padding;
    _titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        textX, self.bounds.size.height - 22, self.bounds.size.width - textX - padding, 18)];
    _titleLabel.bezeled = NO;
    _titleLabel.drawsBackground = NO;
    _titleLabel.editable = NO;
    _titleLabel.selectable = NO;
    _titleLabel.textColor = [DSColors textPrimary];
    _titleLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    _titleLabel.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_titleLabel];

    // URL label
    _urlLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        textX, 4, self.bounds.size.width - textX - padding, 14)];
    _urlLabel.bezeled = NO;
    _urlLabel.drawsBackground = NO;
    _urlLabel.editable = NO;
    _urlLabel.selectable = NO;
    _urlLabel.textColor = [DSColors accent];
    _urlLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _urlLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    _urlLabel.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_urlLabel];
}

- (void)setSuggestion:(AutocompleteSuggestion*)suggestion {
    _suggestion = suggestion;
    _titleLabel.stringValue = suggestion.title ?: @"";
    _urlLabel.stringValue = suggestion.url ?: @"";

    if (suggestion.favicon) {
        _faviconView.image = suggestion.favicon;
    } else {
        _faviconView.image = [NSImage imageWithSystemSymbolName:@"globe"
                                      accessibilityDescription:nil];
    }
}

- (void)setIsSelected:(BOOL)isSelected {
    _isSelected = isSelected;
    [self updateAppearance];
}

- (void)updateAppearance {
    if (_isSelected) {
        self.layer.backgroundColor = [DSColors accent].CGColor;
        _titleLabel.textColor = [NSColor whiteColor];
        _urlLabel.textColor = [[NSColor whiteColor] colorWithAlphaComponent:0.8];
        _faviconView.contentTintColor = [NSColor whiteColor];
    } else if (_isHovered) {
        self.layer.backgroundColor = [DSColors surfaceHover].CGColor;
        _titleLabel.textColor = [DSColors textPrimary];
        _urlLabel.textColor = [DSColors accent];
        _faviconView.contentTintColor = nil;
    } else {
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
        _titleLabel.textColor = [DSColors textPrimary];
        _urlLabel.textColor = [DSColors accent];
        _faviconView.contentTintColor = nil;
    }
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
    if (!_isSelected) {
        [self updateAppearance];
    }
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    [self updateAppearance];
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    if (_onClick) {
        _onClick();
    }
}

@end

// ============================================================================
// AUTOCOMPLETE DROPDOWN VIEW
// ============================================================================

@implementation AutocompleteDropdownView {
    NSScrollView* _scrollView;
    NSView* _contentView;
    NSMutableArray<AutocompleteSuggestion*>* _suggestions;
    NSMutableArray<AutocompleteRowView*>* _rowViews;
    NSInteger _selectedIndex;
}

- (NSArray<AutocompleteSuggestion*>*)suggestions {
    return _suggestions;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = [DSColors surface].CGColor;
        self.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        self.layer.borderWidth = 1;
        self.layer.borderColor = [DSColors border].CGColor;

        // Shadow
        NSShadow* shadow = [[NSShadow alloc] init];
        shadow.shadowColor = [[NSColor blackColor] colorWithAlphaComponent:0.3];
        shadow.shadowOffset = NSMakeSize(0, -4);
        shadow.shadowBlurRadius = 12;
        self.shadow = shadow;

        _suggestions = [NSMutableArray array];
        _rowViews = [NSMutableArray array];
        _selectedIndex = 0;

        [self setupScrollView];
        self.hidden = YES;
    }
    return self;
}

- (void)setupScrollView {
    _scrollView = [[NSScrollView alloc] initWithFrame:self.bounds];
    _scrollView.hasVerticalScroller = YES;
    _scrollView.hasHorizontalScroller = NO;
    _scrollView.autohidesScrollers = YES;
    _scrollView.drawsBackground = NO;
    _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:_scrollView];

    _contentView = [[NSView alloc] initWithFrame:self.bounds];
    _contentView.autoresizingMask = NSViewWidthSizable;
    _scrollView.documentView = _contentView;
}

- (NSString*)getDomainFromURL:(NSString*)urlString {
    // Get just the domain from URL
    NSURL* url = [NSURL URLWithString:urlString];
    if (!url || !url.host) return urlString;
    return [url.host lowercaseString];
}

- (void)updateSuggestionsForQuery:(NSString*)query {
    [_suggestions removeAllObjects];

    if (!query || query.length == 0) {
        [self rebuildRows];
        return;
    }

    NSString* lowercaseQuery = [query lowercaseString];
    NSMutableSet<NSString*>* seenURLs = [NSMutableSet set];

    // Get bookmarks where DOMAIN matches query
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        std::vector<Bookmark> allBookmarks = bookmarks->GetAllBookmarks();
        for (const auto& bm : allBookmarks) {
            if (_suggestions.count >= 8) break;

            NSString* url = [NSString stringWithUTF8String:bm.url.c_str()];
            NSString* title = [NSString stringWithUTF8String:bm.title.c_str()];
            NSString* domain = [self getDomainFromURL:url];

            // Only show if domain contains the query
            if (![domain containsString:lowercaseQuery]) continue;

            // Skip duplicate URLs
            if ([seenURLs containsObject:url]) continue;
            [seenURLs addObject:url];

            AutocompleteSuggestion* suggestion = [[AutocompleteSuggestion alloc] init];
            suggestion.url = url;
            suggestion.title = title.length > 0 ? title : url;
            suggestion.favicon = GetCachedFavicon(url);
            suggestion.isBookmarked = YES;
            [_suggestions addObject:suggestion];
        }
    }

    // Get history entries where DOMAIN matches query
    HistoryStorage* history = GetHistoryStorage();
    if (history) {
        std::vector<HistoryEntry> entries = history->SearchHistory([query UTF8String], 50);

        for (const auto& entry : entries) {
            if (_suggestions.count >= 8) break;

            NSString* url = [NSString stringWithUTF8String:entry.url.c_str()];
            NSString* domain = [self getDomainFromURL:url];

            // Only show if domain contains the query
            if (![domain containsString:lowercaseQuery]) continue;

            // Skip duplicate URLs
            if ([seenURLs containsObject:url]) continue;
            [seenURLs addObject:url];

            AutocompleteSuggestion* suggestion = [[AutocompleteSuggestion alloc] init];
            suggestion.url = url;
            suggestion.title = entry.title.empty() ? url : [NSString stringWithUTF8String:entry.title.c_str()];
            suggestion.favicon = GetCachedFavicon(url);
            suggestion.isBookmarked = NO;
            [_suggestions addObject:suggestion];
        }
    }

    _selectedIndex = _suggestions.count > 0 ? 0 : -1;
    [self rebuildRows];
}

- (void)rebuildRows {
    // Remove old rows
    for (AutocompleteRowView* row in _rowViews) {
        [row removeFromSuperview];
    }
    [_rowViews removeAllObjects];

    CGFloat rowHeight = 44;
    CGFloat y = 0;
    CGFloat width = self.bounds.size.width;

    for (NSUInteger i = 0; i < _suggestions.count; i++) {
        AutocompleteSuggestion* suggestion = _suggestions[i];

        AutocompleteRowView* row = [[AutocompleteRowView alloc] initWithFrame:NSMakeRect(0, y, width, rowHeight)];
        row.suggestion = suggestion;
        row.isSelected = ((NSInteger)i == _selectedIndex);
        row.autoresizingMask = NSViewWidthSizable;

        NSString* urlCopy = suggestion.url;
        void (^onSelectBlock)(NSString*) = self.onSelect;
        row.onClick = ^{
            if (onSelectBlock) {
                onSelectBlock(urlCopy);
            }
        };

        [_contentView addSubview:row];
        [_rowViews addObject:row];
        y += rowHeight;
    }

    // Update content view height
    CGFloat contentHeight = MAX(y, 10);
    _contentView.frame = NSMakeRect(0, 0, width, contentHeight);

    // Resize dropdown to fit content (max 400px)
    CGFloat dropdownHeight = MIN(contentHeight + 8, 400);
    NSRect frame = self.frame;
    frame.size.height = dropdownHeight;
    // Don't modify origin - let the parent handle positioning
    self.frame = frame;

    _scrollView.frame = NSMakeRect(0, 4, frame.size.width, frame.size.height - 8);
}

- (void)show {
    if (_suggestions.count == 0) {
        [self hide];
        return;
    }
    self.hidden = NO;
    NSLog(@"Autocomplete: showing with %lu suggestions, frame: %@", (unsigned long)_suggestions.count, NSStringFromRect(self.frame));
}

- (void)hide {
    self.hidden = YES;
    if (_onDismiss) {
        _onDismiss();
    }
}

- (BOOL)isVisible {
    return !self.hidden;
}

- (void)moveSelectionUp {
    if (_suggestions.count == 0) return;

    NSInteger oldIndex = _selectedIndex;
    _selectedIndex--;
    if (_selectedIndex < 0) {
        _selectedIndex = _suggestions.count - 1;
    }

    [self updateSelectionFrom:oldIndex to:_selectedIndex];
}

- (void)moveSelectionDown {
    if (_suggestions.count == 0) return;

    NSInteger oldIndex = _selectedIndex;
    _selectedIndex++;
    if (_selectedIndex >= (NSInteger)_suggestions.count) {
        _selectedIndex = 0;
    }

    [self updateSelectionFrom:oldIndex to:_selectedIndex];
}

- (void)updateSelectionFrom:(NSInteger)oldIndex to:(NSInteger)newIndex {
    if (oldIndex >= 0 && oldIndex < (NSInteger)_rowViews.count) {
        _rowViews[oldIndex].isSelected = NO;
    }
    if (newIndex >= 0 && newIndex < (NSInteger)_rowViews.count) {
        _rowViews[newIndex].isSelected = YES;

        // Scroll to visible
        NSRect rowFrame = _rowViews[newIndex].frame;
        [_contentView scrollRectToVisible:rowFrame];
    }
}

- (NSString*)selectedURL {
    if (_selectedIndex >= 0 && _selectedIndex < (NSInteger)_suggestions.count) {
        return _suggestions[_selectedIndex].url;
    }
    return nil;
}

@end
