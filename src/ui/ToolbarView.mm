#import "ToolbarView.h"
#import "MainWindowController.h"
#import "Components.h"

// ============================================================================
// TOOLBAR VIEW
// ============================================================================

@implementation ToolbarView {
    DSIconButton* _backButton;
    DSIconButton* _forwardButton;
    DSIconButton* _reloadButton;
    NSTextField* _urlTextField;
    NSImageView* _securityIcon;
    NSView* _urlContainer;
    NSView* _loadingProgressView;
    NSTimer* _loadingAnimationTimer;
    CGFloat _loadingProgress;
    BOOL _isLoading;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupViews];
    }
    return self;
}

- (void)setupViews {
    CGFloat x = [DSSpacing md];
    CGFloat buttonSize = [DSLayout iconSizeLarge];
    CGFloat y = (self.bounds.size.height - buttonSize) / 2;

    // Navigation buttons
    _backButton = [DSIconButton buttonWithIcon:@"chevron.left" tooltip:@"Go Back"];
    _backButton.frame = NSMakeRect(x, y, buttonSize, buttonSize);
    _backButton.target = self;
    _backButton.action = @selector(goBack:);
    _backButton.enabled = NO;
    [self addSubview:_backButton];
    x += buttonSize + [DSSpacing xs];

    _forwardButton = [DSIconButton buttonWithIcon:@"chevron.right" tooltip:@"Go Forward"];
    _forwardButton.frame = NSMakeRect(x, y, buttonSize, buttonSize);
    _forwardButton.target = self;
    _forwardButton.action = @selector(goForward:);
    _forwardButton.enabled = NO;
    [self addSubview:_forwardButton];
    x += buttonSize + [DSSpacing xs];

    _reloadButton = [DSIconButton buttonWithIcon:@"arrow.clockwise" tooltip:@"Reload"];
    _reloadButton.frame = NSMakeRect(x, y, buttonSize, buttonSize);
    _reloadButton.target = self;
    _reloadButton.action = @selector(reload:);
    [self addSubview:_reloadButton];
    x += buttonSize + [DSSpacing md];

    // URL container
    CGFloat containerPadding = 7;
    CGFloat containerHeight = self.bounds.size.height - (containerPadding * 2);
    CGFloat containerWidth = self.bounds.size.width - x - [DSSpacing md];

    _urlContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, containerPadding, containerWidth, containerHeight)];
    _urlContainer.wantsLayer = YES;
    _urlContainer.layer.backgroundColor = [DSColors surface].CGColor;
    _urlContainer.layer.cornerRadius = [DSLayout cornerRadiusMedium];
    _urlContainer.layer.masksToBounds = YES;
    _urlContainer.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_urlContainer];

    // Loading progress view
    _loadingProgressView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, containerHeight)];
    _loadingProgressView.wantsLayer = YES;
    _loadingProgressView.layer.backgroundColor = [DSColors accentSubtle].CGColor;
    _loadingProgressView.hidden = YES;
    [_urlContainer addSubview:_loadingProgressView positioned:NSWindowBelow relativeTo:nil];

    // Security icon
    CGFloat iconSize = [DSLayout iconSizeSmall];
    CGFloat iconX = [DSSpacing sm];
    CGFloat iconY = (containerHeight - iconSize) / 2;
    _securityIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _securityIcon.imageScaling = NSImageScaleProportionallyUpOrDown;
    _securityIcon.hidden = YES;
    [_urlContainer addSubview:_securityIcon];

    // URL text field
    CGFloat textFieldLeftInset = iconX + iconSize + [DSSpacing sm];
    CGFloat textFieldHeight = 20;
    CGFloat textFieldY = (containerHeight - textFieldHeight) / 2 - 2;

    NSTextField* textField = [[NSTextField alloc] initWithFrame:NSMakeRect(
        textFieldLeftInset, textFieldY,
        containerWidth - textFieldLeftInset - [DSSpacing sm], textFieldHeight)];
    textField.bezeled = NO;
    textField.drawsBackground = NO;
    textField.backgroundColor = [NSColor clearColor];
    textField.textColor = [DSColors textPrimary];
    textField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    textField.focusRingType = NSFocusRingTypeNone;
    textField.delegate = self;
    textField.placeholderString = @"Search or enter URL";
    textField.autoresizingMask = NSViewWidthSizable;

    NSTextFieldCell* cell = textField.cell;
    cell.truncatesLastVisibleLine = YES;
    cell.lineBreakMode = NSLineBreakByTruncatingTail;
    cell.wraps = NO;
    cell.scrollable = YES;

    _urlTextField = textField;
    [_urlContainer addSubview:textField];
}

#pragma mark - Actions

- (void)goBack:(id)sender {
    (void)sender;
    [_windowController goBack];
}

- (void)goForward:(id)sender {
    (void)sender;
    [_windowController goForward];
}

- (void)reload:(id)sender {
    (void)sender;
    [_windowController reload];
}

#pragma mark - Public Methods

- (void)setURL:(NSString*)url {
    _urlTextField.stringValue = url ?: @"";

    if (!url || url.length == 0) {
        _securityIcon.hidden = YES;
    } else if ([url hasPrefix:@"https://"]) {
        _securityIcon.image = [NSImage imageWithSystemSymbolName:@"lock.fill"
                                        accessibilityDescription:@"Secure"];
        _securityIcon.contentTintColor = [DSColors success];
        _securityIcon.hidden = NO;
    } else if ([url hasPrefix:@"http://"]) {
        _securityIcon.image = [NSImage imageWithSystemSymbolName:@"exclamationmark.triangle.fill"
                                        accessibilityDescription:@"Not Secure"];
        _securityIcon.contentTintColor = [DSColors warning];
        _securityIcon.hidden = NO;
    } else {
        _securityIcon.hidden = YES;
    }
}

- (void)setCanGoBack:(BOOL)canGoBack canGoForward:(BOOL)canGoForward {
    _backButton.enabled = canGoBack;
    _forwardButton.enabled = canGoForward;
}

- (void)setLoading:(BOOL)isLoading {
    if (isLoading && !_isLoading) {
        _isLoading = YES;
        _loadingProgress = 0.0;
        _loadingProgressView.hidden = NO;

        [_loadingAnimationTimer invalidate];
        _loadingAnimationTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                                 repeats:YES
                                                                   block:^(NSTimer* timer) {
            (void)timer;
            if (self->_loadingProgress < 0.9) {
                self->_loadingProgress += 0.03;
            } else if (self->_loadingProgress < 0.98) {
                self->_loadingProgress += 0.002;
            }
            [self updateLoadingProgressView];
        }];
    } else if (!isLoading && _isLoading) {
        _isLoading = NO;
        [_loadingAnimationTimer invalidate];
        _loadingAnimationTimer = nil;

        _loadingProgress = 1.0;
        [self updateLoadingProgressView];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [DSAnimation animateWithDuration:[DSAnimation durationNormal]
                                  animations:^{
                self->_loadingProgressView.alphaValue = 0.0;
            } completion:^{
                self->_loadingProgressView.hidden = YES;
                self->_loadingProgressView.alphaValue = 1.0;
                self->_loadingProgress = 0.0;
                [self updateLoadingProgressView];
            }];
        });
    }
}

- (void)updateLoadingProgressView {
    CGFloat width = _urlContainer.bounds.size.width * _loadingProgress;
    _loadingProgressView.frame = NSMakeRect(0, 0, width, _urlContainer.bounds.size.height);
}

- (void)focusURLField {
    [self.window makeFirstResponder:_urlTextField];
    [_urlTextField selectText:nil];

    // Show focus ring
    _urlContainer.layer.borderWidth = 1.0;
    _urlContainer.layer.borderColor = [DSColors accent].CGColor;
}

#pragma mark - NSTextFieldDelegate

- (void)controlTextDidBeginEditing:(NSNotification*)notification {
    (void)notification;
    _urlContainer.layer.borderWidth = 1.0;
    _urlContainer.layer.borderColor = [DSColors accent].CGColor;
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
    NSTextField* textField = notification.object;
    if (textField == _urlTextField) {
        NSString* url = _urlTextField.stringValue;
        if (url.length > 0) {
            [_windowController navigateToURL:url];
        }
        _urlContainer.layer.borderWidth = 0;
    }
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector {
    (void)control;
    (void)textView;
    if (commandSelector == @selector(insertNewline:)) {
        NSString* url = _urlTextField.stringValue;
        if (url.length > 0) {
            [_windowController navigateToURL:url];
        }
        [self.window makeFirstResponder:nil];
        _urlContainer.layer.borderWidth = 0;
        return YES;
    }
    return NO;
}

#pragma mark - Drawing

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[DSColors background] setFill];
    NSRectFill(self.bounds);

    // Top separator
    [[DSColors border] setFill];
    NSRectFill(NSMakeRect(0, self.bounds.size.height - 1, self.bounds.size.width, 1));
}

@end
