#import "DSTextField.h"
#import "DesignSystem.h"

// ============================================================================
// DS TEXT FIELD IMPLEMENTATION
// ============================================================================

@implementation DSTextField {
    NSTextField* _textField;
    BOOL _isFocused;
}

@synthesize textField = _textField;
@synthesize isFocused = _isFocused;

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupView];
    }
    return self;
}

- (void)setupView {
    self.wantsLayer = YES;
    self.layer.backgroundColor = [DSColors surface].CGColor;
    self.layer.cornerRadius = [DSLayout cornerRadiusMedium];
    self.layer.borderWidth = 0;

    // Create text field
    CGFloat padding = [DSSpacing sm];
    NSRect textFrame = NSInsetRect(self.bounds, padding, 0);
    textFrame.origin.y = (self.bounds.size.height - 20) / 2;
    textFrame.size.height = 20;

    _textField = [[NSTextField alloc] initWithFrame:textFrame];
    _textField.bezeled = NO;
    _textField.drawsBackground = NO;
    _textField.backgroundColor = [NSColor clearColor];
    _textField.textColor = [DSColors textPrimary];
    _textField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _textField.focusRingType = NSFocusRingTypeNone;
    _textField.delegate = self;
    _textField.autoresizingMask = NSViewWidthSizable;

    NSTextFieldCell* cell = _textField.cell;
    cell.truncatesLastVisibleLine = YES;
    cell.lineBreakMode = NSLineBreakByTruncatingTail;
    cell.wraps = NO;
    cell.scrollable = YES;

    [self addSubview:_textField];
}

#pragma mark - Properties

- (NSString*)text {
    return _textField.stringValue;
}

- (void)setText:(NSString*)text {
    _textField.stringValue = text ?: @"";
}

- (NSString*)placeholder {
    return _textField.placeholderString;
}

- (void)setPlaceholder:(NSString*)placeholder {
    _textField.placeholderString = placeholder;
}

#pragma mark - Focus

- (void)focus {
    [self.window makeFirstResponder:_textField];
}

- (void)blur {
    [self.window makeFirstResponder:nil];
}

- (void)selectAll {
    [_textField selectText:nil];
}

- (void)updateFocusAppearance {
    if (_isFocused) {
        self.layer.borderWidth = 1.0;
        self.layer.borderColor = [DSColors accent].CGColor;
    } else {
        self.layer.borderWidth = 0;
    }
}

#pragma mark - NSTextFieldDelegate

- (void)controlTextDidBeginEditing:(NSNotification*)notification {
    (void)notification;
    _isFocused = YES;
    [self updateFocusAppearance];
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
    (void)notification;
    _isFocused = NO;
    [self updateFocusAppearance];
}

- (void)controlTextDidChange:(NSNotification*)notification {
    (void)notification;
    if (_onTextChange) {
        _onTextChange(_textField.stringValue);
    }
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector {
    (void)control;
    (void)textView;
    if (commandSelector == @selector(insertNewline:)) {
        if (_onSubmit) {
            _onSubmit(_textField.stringValue);
        }
        [self blur];
        return YES;
    }
    return NO;
}

#pragma mark - Drawing

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Background is handled by layer
}

@end

// ============================================================================
// DS SEARCH FIELD IMPLEMENTATION
// ============================================================================

@implementation DSSearchField {
    NSImageView* _searchIcon;
    NSButton* _clearButton;
}

- (void)setupView {
    [super setupView];

    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat padding = [DSSpacing sm];

    // Search icon
    _searchIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(
        padding, (self.bounds.size.height - iconSize) / 2, iconSize, iconSize)];
    _searchIcon.image = [NSImage imageWithSystemSymbolName:@"magnifyingglass"
                                  accessibilityDescription:@"Search"];
    _searchIcon.contentTintColor = [DSColors textSecondary];
    _searchIcon.imageScaling = NSImageScaleProportionallyUpOrDown;
    [self addSubview:_searchIcon];

    // Clear button (initially hidden)
    CGFloat clearButtonSize = iconSize;
    _clearButton = [[NSButton alloc] initWithFrame:NSMakeRect(
        self.bounds.size.width - padding - clearButtonSize,
        (self.bounds.size.height - clearButtonSize) / 2,
        clearButtonSize, clearButtonSize)];
    _clearButton.bezelStyle = NSBezelStyleInline;
    _clearButton.bordered = NO;
    _clearButton.image = [NSImage imageWithSystemSymbolName:@"xmark.circle.fill"
                                   accessibilityDescription:@"Clear"];
    _clearButton.contentTintColor = [DSColors textSecondary];
    _clearButton.target = self;
    _clearButton.action = @selector(clearText:);
    _clearButton.hidden = YES;
    [self addSubview:_clearButton];

    // Adjust text field position
    CGFloat textX = padding + iconSize + padding;
    NSRect textFrame = self.textField.frame;
    textFrame.origin.x = textX;
    textFrame.size.width = self.bounds.size.width - textX - padding;
    self.textField.frame = textFrame;
}

- (void)setShowsClearButton:(BOOL)showsClearButton {
    _showsClearButton = showsClearButton;
    [self updateClearButtonVisibility];
}

- (void)updateClearButtonVisibility {
    BOOL shouldShow = _showsClearButton && self.textField.stringValue.length > 0;
    _clearButton.hidden = !shouldShow;

    // Adjust text field width when clear button is visible
    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat padding = [DSSpacing sm];
    CGFloat textX = padding + iconSize + padding;
    CGFloat rightPadding = shouldShow ? (padding + iconSize + padding) : padding;

    NSRect textFrame = self.textField.frame;
    textFrame.origin.x = textX;
    textFrame.size.width = self.bounds.size.width - textX - rightPadding;
    self.textField.frame = textFrame;
}

- (void)clearText:(id)sender {
    (void)sender;
    self.textField.stringValue = @"";
    [self updateClearButtonVisibility];

    if (self.onTextChange) {
        self.onTextChange(@"");
    }
}

- (void)controlTextDidChange:(NSNotification*)notification {
    [super controlTextDidChange:notification];
    [self updateClearButtonVisibility];
}

@end
