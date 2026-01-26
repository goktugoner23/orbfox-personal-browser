#import "DSRow.h"
#import "DesignSystem.h"
#import "DSButton.h"

// ============================================================================
// DS ROW IMPLEMENTATION
// ============================================================================

@implementation DSRow {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    NSImageView* _iconView;
    NSTextField* _titleLabel;
    NSTextField* _subtitleLabel;
    DSIconButton* _closeButton;
}

@synthesize isHovered = _isHovered;

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit {
    self.wantsLayer = YES;
    self.layer.cornerRadius = [DSLayout cornerRadiusMedium];
    [self setupSubviews];
    [self updateAppearance];
}

- (void)setupSubviews {
    CGFloat padding = [DSSpacing sm];
    CGFloat iconSize = [DSLayout iconSizeSmall];

    // Icon view
    _iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
        padding, (self.bounds.size.height - iconSize) / 2, iconSize, iconSize)];
    _iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
    _iconView.hidden = YES;
    [self addSubview:_iconView];

    // Title label
    CGFloat titleX = padding;
    _titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        titleX, (self.bounds.size.height - 18) / 2,
        self.bounds.size.width - titleX - padding - 30, 18)];
    _titleLabel.bezeled = NO;
    _titleLabel.drawsBackground = NO;
    _titleLabel.editable = NO;
    _titleLabel.selectable = NO;
    _titleLabel.textColor = [DSColors textPrimary];
    _titleLabel.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    _titleLabel.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_titleLabel];

    // Close button (hidden by default)
    CGFloat closeSize = 20;
    _closeButton = [DSIconButton buttonWithIcon:@"xmark"];
    _closeButton.frame = NSMakeRect(
        self.bounds.size.width - closeSize - padding,
        (self.bounds.size.height - closeSize) / 2,
        closeSize, closeSize);
    _closeButton.autoresizingMask = NSViewMinXMargin;
    _closeButton.hidden = YES;
    _closeButton.target = self;
    _closeButton.action = @selector(closeButtonClicked:);
    [self addSubview:_closeButton];
}

#pragma mark - Properties

- (void)setIcon:(NSImage*)icon {
    _icon = icon;
    _iconView.image = icon;
    _iconView.hidden = (icon == nil);
    [self updateLayout];
}

- (void)setTitle:(NSString*)title {
    _title = [title copy];
    _titleLabel.stringValue = title ?: @"";
}

- (void)setSubtitle:(NSString*)subtitle {
    _subtitle = [subtitle copy];
    if (!_subtitleLabel && subtitle) {
        [self createSubtitleLabel];
    }
    _subtitleLabel.stringValue = subtitle ?: @"";
    _subtitleLabel.hidden = (subtitle == nil || subtitle.length == 0);
    [self updateLayout];
}

- (void)setIsSelected:(BOOL)isSelected {
    _isSelected = isSelected;
    [self updateAppearance];
}

- (void)setShowsCloseButton:(BOOL)showsCloseButton {
    _showsCloseButton = showsCloseButton;
    _closeButton.hidden = !showsCloseButton || !_isHovered;
}

- (void)createSubtitleLabel {
    CGFloat padding = [DSSpacing sm];
    _subtitleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        padding, 4, self.bounds.size.width - padding * 2 - 50, 14)];
    _subtitleLabel.bezeled = NO;
    _subtitleLabel.drawsBackground = NO;
    _subtitleLabel.editable = NO;
    _subtitleLabel.selectable = NO;
    _subtitleLabel.textColor = [DSColors textSecondary];
    _subtitleLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    _subtitleLabel.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_subtitleLabel];
}

- (void)updateLayout {
    CGFloat padding = [DSSpacing sm];
    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat titleX = padding;

    if (_icon) {
        titleX = padding + iconSize + padding;
    }

    if (_subtitle && _subtitle.length > 0) {
        // Two-line layout
        _titleLabel.frame = NSMakeRect(titleX, 18,
            self.bounds.size.width - titleX - padding - 30, 18);
        _subtitleLabel.frame = NSMakeRect(titleX, 4,
            self.bounds.size.width - titleX - padding - 50, 14);
    } else {
        // Single-line centered
        _titleLabel.frame = NSMakeRect(titleX, (self.bounds.size.height - 18) / 2,
            self.bounds.size.width - titleX - padding - 30, 18);
    }
}

#pragma mark - Appearance

- (void)updateAppearance {
    NSColor* bgColor;
    if (_isSelected) {
        bgColor = [DSColors surfaceActive];
    } else if (_isHovered) {
        bgColor = [DSColors surfaceHover];
    } else {
        bgColor = [NSColor clearColor];
    }
    self.layer.backgroundColor = bgColor.CGColor;
}

#pragma mark - Tracking

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
    if (_showsCloseButton) {
        _closeButton.hidden = NO;
    }
    [self updateAppearance];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    _closeButton.hidden = YES;
    [self updateAppearance];
}

- (void)mouseDown:(NSEvent*)event {
    (void)event;
    if (_onClick) {
        _onClick();
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    if (_onRightClick) {
        _onRightClick(event);
    }
}

- (void)closeButtonClicked:(id)sender {
    (void)sender;
    if (_onClose) {
        _onClose();
    }
}

@end

// ============================================================================
// DS TAB ROW IMPLEMENTATION
// ============================================================================

@implementation DSTabRow {
    NSImageView* _faviconView;
    NSProgressIndicator* _loadingIndicator;
}

- (void)setupSubviews {
    [super setupSubviews];

    CGFloat padding = [DSSpacing sm];
    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat iconY = (self.bounds.size.height - iconSize) / 2;

    // Favicon view
    _faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
        padding, iconY, iconSize, iconSize)];
    _faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
    _faviconView.hidden = YES;
    [self addSubview:_faviconView];

    // Loading indicator
    _loadingIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(
        padding, iconY, iconSize, iconSize)];
    _loadingIndicator.style = NSProgressIndicatorStyleSpinning;
    _loadingIndicator.controlSize = NSControlSizeSmall;
    _loadingIndicator.displayedWhenStopped = NO;
    [self addSubview:_loadingIndicator];

    self.showsCloseButton = YES;
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
    [self updateLayout];
}

- (void)setFavicon:(NSImage*)favicon {
    _favicon = favicon;
    _faviconView.image = favicon;
    if (!_isLoading) {
        _faviconView.hidden = (favicon == nil);
    }
    [self updateLayout];
}

- (void)updateLayout {
    CGFloat padding = [DSSpacing sm];
    CGFloat iconSize = [DSLayout iconSizeSmall];
    BOOL hasIcon = _isLoading || _favicon != nil;

    CGFloat titleX = hasIcon ? (padding + iconSize + padding) : padding;

    // Update title position
    NSTextField* titleLabel = self.subviews[2]; // After iconView and faviconView
    if ([titleLabel isKindOfClass:[NSTextField class]]) {
        NSRect frame = titleLabel.frame;
        frame.origin.x = titleX;
        frame.size.width = self.bounds.size.width - titleX - padding - 30;
        titleLabel.frame = frame;
    }
}

@end

// ============================================================================
// DS HISTORY ROW IMPLEMENTATION
// ============================================================================

@implementation DSHistoryRow {
    NSTextField* _timeLabel;
}

- (void)setupSubviews {
    [super setupSubviews];

    // Time label (right-aligned)
    CGFloat padding = [DSSpacing sm];
    _timeLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(
        self.bounds.size.width - 55, (self.bounds.size.height - 14) / 2, 50, 14)];
    _timeLabel.bezeled = NO;
    _timeLabel.drawsBackground = NO;
    _timeLabel.editable = NO;
    _timeLabel.selectable = NO;
    _timeLabel.textColor = [DSColors textSecondary];
    _timeLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _timeLabel.alignment = NSTextAlignmentRight;
    _timeLabel.autoresizingMask = NSViewMinXMargin;
    [self addSubview:_timeLabel];
}

- (void)setUrl:(NSString*)url {
    _url = [url copy];
    self.subtitle = url;
}

- (void)setTime:(NSString*)time {
    _time = [time copy];
    _timeLabel.stringValue = time ?: @"";
}

@end
