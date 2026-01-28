#import "FindBarView.h"
#import "MainWindowController.h"
#import "components/Components.h"

@implementation FindBarView {
    NSTextField* _searchField;
    NSTextField* _matchCountLabel;
    DSIconButton* _prevButton;
    DSIconButton* _nextButton;
    DSIconButton* _closeButton;
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
        shadow.shadowOffset = NSMakeSize(0, -2);
        shadow.shadowBlurRadius = 8;
        self.shadow = shadow;

        [self setupSubviews];
    }
    return self;
}

- (void)setupSubviews {
    CGFloat padding = [DSSpacing sm];
    CGFloat height = self.bounds.size.height;
    CGFloat x = padding;

    // Search field
    CGFloat fieldWidth = 160;
    _searchField = [[NSTextField alloc] initWithFrame:NSMakeRect(x, (height - 24) / 2, fieldWidth, 24)];
    _searchField.placeholderString = @"Find in page...";
    _searchField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _searchField.bezelStyle = NSTextFieldRoundedBezel;
    _searchField.delegate = self;
    [self addSubview:_searchField];
    x += fieldWidth + padding;

    // Match count label
    _matchCountLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(x, (height - 18) / 2, 70, 18)];
    _matchCountLabel.stringValue = @"";
    _matchCountLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _matchCountLabel.textColor = [DSColors textSecondary];
    _matchCountLabel.bezeled = NO;
    _matchCountLabel.drawsBackground = NO;
    _matchCountLabel.editable = NO;
    _matchCountLabel.selectable = NO;
    _matchCountLabel.alignment = NSTextAlignmentCenter;
    [self addSubview:_matchCountLabel];
    x += 70 + padding / 2;

    // Previous button
    _prevButton = [DSIconButton buttonWithIcon:@"chevron.up"];
    _prevButton.frame = NSMakeRect(x, (height - 24) / 2, 24, 24);
    _prevButton.target = self;
    _prevButton.action = @selector(prevClicked:);
    _prevButton.toolTip = @"Previous match (Shift+Enter)";
    [self addSubview:_prevButton];
    x += 24 + 2;

    // Next button
    _nextButton = [DSIconButton buttonWithIcon:@"chevron.down"];
    _nextButton.frame = NSMakeRect(x, (height - 24) / 2, 24, 24);
    _nextButton.target = self;
    _nextButton.action = @selector(nextClicked:);
    _nextButton.toolTip = @"Next match (Enter)";
    [self addSubview:_nextButton];
    x += 24 + padding;

    // Close button
    _closeButton = [DSIconButton buttonWithIcon:@"xmark"];
    _closeButton.frame = NSMakeRect(x, (height - 24) / 2, 24, 24);
    _closeButton.target = self;
    _closeButton.action = @selector(closeClicked:);
    _closeButton.toolTip = @"Close (Escape)";
    [self addSubview:_closeButton];
}

- (void)focusSearchField {
    [self.window makeFirstResponder:_searchField];
    // Select all text if there is any
    if (_searchField.stringValue.length > 0) {
        [_searchField selectText:nil];
    }
}

- (NSString*)searchText {
    return _searchField.stringValue;
}

- (void)updateMatchCount:(int)count activeMatch:(int)activeMatch {
    if (count == 0 && _searchField.stringValue.length > 0) {
        _matchCountLabel.stringValue = @"No matches";
        _matchCountLabel.textColor = [NSColor systemRedColor];
    } else if (count > 0) {
        _matchCountLabel.stringValue = [NSString stringWithFormat:@"%d of %d", activeMatch, count];
        _matchCountLabel.textColor = [DSColors textSecondary];
    } else {
        _matchCountLabel.stringValue = @"";
    }
}

#pragma mark - Actions

- (void)prevClicked:(id)sender {
    (void)sender;
    if (_onPrev) {
        _onPrev();
    }
}

- (void)nextClicked:(id)sender {
    (void)sender;
    if (_onNext) {
        _onNext();
    }
}

- (void)closeClicked:(id)sender {
    (void)sender;
    if (_onClose) {
        _onClose();
    }
}

#pragma mark - NSTextFieldDelegate

- (void)controlTextDidChange:(NSNotification*)notification {
    (void)notification;
    if (_onSearchChanged) {
        _onSearchChanged(_searchField.stringValue);
    }
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector {
    (void)control;
    (void)textView;

    if (commandSelector == @selector(cancelOperation:)) {
        // Escape key
        if (_onClose) {
            _onClose();
        }
        return YES;
    } else if (commandSelector == @selector(insertNewline:)) {
        // Enter key - find next
        if (_onNext) {
            _onNext();
        }
        return YES;
    }
    return NO;
}

// Handle Shift+Enter for previous match
- (void)keyDown:(NSEvent*)event {
    if (event.keyCode == 36 && (event.modifierFlags & NSEventModifierFlagShift)) {
        // Shift+Enter
        if (_onPrev) {
            _onPrev();
        }
    } else {
        [super keyDown:event];
    }
}

@end
