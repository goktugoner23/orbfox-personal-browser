#import "DSButton.h"
#import "DesignSystem.h"

// ============================================================================
// DS BUTTON IMPLEMENTATION
// ============================================================================

@implementation DSButton {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    BOOL _isPressed;
}

@synthesize isHovered = _isHovered;

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit {
    _variant = DSButtonVariantGhost;
    _size = DSButtonSizeMedium;
    _isHovered = NO;
    _isPressed = NO;

    self.bezelStyle = NSBezelStyleInline;
    self.bordered = NO;
    self.wantsLayer = YES;
    self.layer.cornerRadius = [DSLayout cornerRadiusMedium];

    [self updateAppearance];
}

#pragma mark - Factory Methods

+ (instancetype)buttonWithTitle:(NSString*)title variant:(DSButtonVariant)variant {
    DSButton* button = [[self alloc] initWithFrame:NSZeroRect];
    button.title = title;
    button.variant = variant;
    [button sizeToFit];
    return button;
}

+ (instancetype)buttonWithIcon:(NSString*)symbolName variant:(DSButtonVariant)variant {
    DSButton* button = [[self alloc] initWithFrame:NSZeroRect];
    button.image = [NSImage imageWithSystemSymbolName:symbolName
                             accessibilityDescription:symbolName];
    button.imagePosition = NSImageOnly;
    button.variant = variant;
    [button sizeToFit];
    return button;
}

+ (instancetype)buttonWithTitle:(NSString*)title
                           icon:(NSString*)symbolName
                        variant:(DSButtonVariant)variant {
    DSButton* button = [[self alloc] initWithFrame:NSZeroRect];
    button.title = title;
    button.image = [NSImage imageWithSystemSymbolName:symbolName
                             accessibilityDescription:symbolName];
    button.imagePosition = NSImageLeading;
    button.variant = variant;
    [button sizeToFit];
    return button;
}

#pragma mark - Properties

- (void)setVariant:(DSButtonVariant)variant {
    _variant = variant;
    [self updateAppearance];
}

- (void)setSize:(DSButtonSize)size {
    _size = size;
    [self updateAppearance];
}

- (void)setEnabled:(BOOL)enabled {
    [super setEnabled:enabled];
    [self updateAppearance];
}

#pragma mark - Appearance

- (void)updateAppearance {
    NSColor* bgColor = nil;
    NSColor* tintColor = nil;

    if (!self.isEnabled) {
        bgColor = [NSColor clearColor];
        tintColor = [DSColors textDisabled];
    } else if (_isPressed) {
        switch (_variant) {
            case DSButtonVariantGhost:
            case DSButtonVariantSubtle:
                bgColor = [DSColors surfaceActive];
                tintColor = [DSColors textPrimary];
                break;
            case DSButtonVariantFilled:
                bgColor = [DSColors accentHover];
                tintColor = [NSColor whiteColor];
                break;
        }
    } else if (_isHovered) {
        switch (_variant) {
            case DSButtonVariantGhost:
                bgColor = [DSColors surfaceHover];
                tintColor = [DSColors textPrimary];
                break;
            case DSButtonVariantSubtle:
                bgColor = [DSColors surfaceHover];
                tintColor = [DSColors textPrimary];
                break;
            case DSButtonVariantFilled:
                bgColor = [DSColors accentHover];
                tintColor = [NSColor whiteColor];
                break;
        }
    } else {
        switch (_variant) {
            case DSButtonVariantGhost:
                bgColor = [NSColor clearColor];
                tintColor = [DSColors textSecondary];
                break;
            case DSButtonVariantSubtle:
                bgColor = [DSColors surface];
                tintColor = [DSColors textPrimary];
                break;
            case DSButtonVariantFilled:
                bgColor = [DSColors accent];
                tintColor = [NSColor whiteColor];
                break;
        }
    }

    self.layer.backgroundColor = bgColor.CGColor;
    self.contentTintColor = tintColor;

    // Update font based on size
    NSFont* font;
    switch (_size) {
        case DSButtonSizeSmall:
            font = [DSTypography fontWithStyle:DSFontStyleCaption];
            break;
        case DSButtonSizeMedium:
            font = [DSTypography fontWithStyle:DSFontStyleBody];
            break;
        case DSButtonSizeLarge:
            font = [DSTypography fontWithStyle:DSFontStyleBodyMedium];
            break;
    }
    self.font = font;
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
    [self updateAppearance];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    [self updateAppearance];
}

- (void)mouseDown:(NSEvent*)event {
    _isPressed = YES;
    [self updateAppearance];
    [super mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event {
    _isPressed = NO;
    [self updateAppearance];
    [super mouseUp:event];
}

- (void)resetHoverState {
    _isHovered = NO;
    _isPressed = NO;
    [self updateAppearance];
}

@end

// ============================================================================
// DS ICON BUTTON IMPLEMENTATION
// ============================================================================

@implementation DSIconButton {
    BOOL _selected;
    BOOL _showsHoverBackground;
}

- (void)commonInit {
    [super commonInit];
    self.imagePosition = NSImageOnly;
    self.size = DSButtonSizeMedium;
    _selected = NO;
    _showsHoverBackground = YES;  // Default to showing hover background
}

+ (instancetype)buttonWithIcon:(NSString*)symbolName {
    return [self buttonWithIcon:symbolName tooltip:nil];
}

+ (instancetype)buttonWithIcon:(NSString*)symbolName tooltip:(NSString*)tooltip {
    DSIconButton* button = [[self alloc] initWithFrame:NSMakeRect(0, 0,
        [DSLayout iconSizeLarge], [DSLayout iconSizeLarge])];
    button.symbolName = symbolName;
    button.image = [NSImage imageWithSystemSymbolName:symbolName
                             accessibilityDescription:symbolName];
    if (tooltip) {
        button.toolTip = tooltip;
    }
    return button;
}

- (void)setSymbolName:(NSString*)symbolName {
    _symbolName = [symbolName copy];
    self.image = [NSImage imageWithSystemSymbolName:symbolName
                           accessibilityDescription:symbolName];
}

- (BOOL)selected {
    return _selected;
}

- (void)setSelected:(BOOL)selected {
    _selected = selected;
    [self updateAppearance];
}

- (BOOL)showsHoverBackground {
    return _showsHoverBackground;
}

- (void)setShowsHoverBackground:(BOOL)showsHoverBackground {
    _showsHoverBackground = showsHoverBackground;
    [self updateAppearance];
}

- (void)updateAppearance {
    // If selected, show hover-style background (persists even when not hovering)
    if (_selected) {
        self.layer.backgroundColor = [DSColors surfaceHover].CGColor;
        self.contentTintColor = [DSColors textPrimary];
        return;
    }

    // If hover background disabled, clear background and just update tint
    if (!_showsHoverBackground) {
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
        if (self.isHovered) {
            self.contentTintColor = [DSColors textPrimary];
        } else {
            self.contentTintColor = [DSColors textSecondary];
        }
        return;
    }

    // Otherwise use parent's appearance logic
    [super updateAppearance];
}

@end
