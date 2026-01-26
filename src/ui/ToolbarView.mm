#import "ToolbarView.h"
#import "MainWindowController.h"

// Custom cell that centers text vertically
@interface VerticalCenterTextFieldCell : NSTextFieldCell
@end

@implementation VerticalCenterTextFieldCell

- (NSRect)adjustedFrameToVerticallyCenterText:(NSRect)frame {
    NSAttributedString* attrString = self.attributedStringValue;
    if (attrString.length == 0) {
        // Use placeholder for sizing when empty
        attrString = self.placeholderAttributedString;
        if (!attrString) {
            attrString = [[NSAttributedString alloc] initWithString:self.placeholderString ?: @"X"
                                                         attributes:@{NSFontAttributeName: self.font}];
        }
    }
    NSRect textRect = [attrString boundingRectWithSize:frame.size options:0 context:nil];
    CGFloat heightDelta = frame.size.height - textRect.size.height;
    if (heightDelta > 0) {
        frame.origin.y += heightDelta / 2.0;
        frame.size.height -= heightDelta;
    }
    return frame;
}

- (void)editWithFrame:(NSRect)rect inView:(NSView*)controlView editor:(NSText*)textObj delegate:(id)delegate event:(NSEvent*)event {
    [super editWithFrame:[self adjustedFrameToVerticallyCenterText:rect] inView:controlView editor:textObj delegate:delegate event:event];
}

- (void)selectWithFrame:(NSRect)rect inView:(NSView*)controlView editor:(NSText*)textObj delegate:(id)delegate start:(NSInteger)start length:(NSInteger)length {
    [super selectWithFrame:[self adjustedFrameToVerticallyCenterText:rect] inView:controlView editor:textObj delegate:delegate start:start length:length];
}

- (void)drawInteriorWithFrame:(NSRect)frame inView:(NSView*)controlView {
    [super drawInteriorWithFrame:[self adjustedFrameToVerticallyCenterText:frame] inView:controlView];
}

@end

static NSColor* ToolbarBackgroundColor() {
    return [NSColor colorWithRed:0.11 green:0.11 blue:0.12 alpha:1.0];
}

static NSColor* URLFieldBackgroundColor() {
    return [NSColor colorWithRed:0.18 green:0.18 blue:0.20 alpha:1.0];
}

static NSColor* TextColor() {
    return [NSColor colorWithRed:0.9 green:0.9 blue:0.9 alpha:1.0];
}

static NSColor* SecondaryTextColor() {
    return [NSColor colorWithRed:0.6 green:0.6 blue:0.6 alpha:1.0];
}

static NSColor* DisabledColor() {
    return [NSColor colorWithRed:0.35 green:0.35 blue:0.38 alpha:1.0];
}

static const CGFloat kToolbarHeight = 44.0;
static const CGFloat kButtonSize = 28.0;
static const CGFloat kButtonSpacing = 4.0;

@implementation ToolbarView {
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
    CGFloat x = 12;
    CGFloat y = (self.bounds.size.height - kButtonSize) / 2;

    // Back button
    _backButton = [self createNavButton:@"chevron.left" frame:NSMakeRect(x, y, kButtonSize, kButtonSize)];
    _backButton.action = @selector(goBack:);
    _backButton.enabled = NO;
    [self addSubview:_backButton];
    x += kButtonSize + kButtonSpacing;

    // Forward button
    _forwardButton = [self createNavButton:@"chevron.right" frame:NSMakeRect(x, y, kButtonSize, kButtonSize)];
    _forwardButton.action = @selector(goForward:);
    _forwardButton.enabled = NO;
    [self addSubview:_forwardButton];
    x += kButtonSize + kButtonSpacing;

    // Reload button
    _reloadButton = [self createNavButton:@"arrow.clockwise" frame:NSMakeRect(x, y, kButtonSize, kButtonSize)];
    _reloadButton.action = @selector(reload:);
    [self addSubview:_reloadButton];
    x += kButtonSize + 12;

    // URL field container - provides the visible background
    CGFloat containerPadding = 7;  // Increased padding
    CGFloat containerHeight = self.bounds.size.height - (containerPadding * 2);
    CGFloat containerWidth = self.bounds.size.width - x - 12;

    _urlContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, containerPadding, containerWidth, containerHeight)];
    _urlContainer.wantsLayer = YES;
    _urlContainer.layer.backgroundColor = URLFieldBackgroundColor().CGColor;
    _urlContainer.layer.cornerRadius = 6;
    _urlContainer.layer.masksToBounds = YES;
    _urlContainer.autoresizingMask = NSViewWidthSizable;
    [self addSubview:_urlContainer];

    // Loading progress view - fills from left to right inside the URL container
    _loadingProgressView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, containerHeight)];
    _loadingProgressView.wantsLayer = YES;
    _loadingProgressView.layer.backgroundColor = [NSColor colorWithRed:0.0 green:0.48 blue:1.0 alpha:0.25].CGColor;
    _loadingProgressView.hidden = YES;
    [_urlContainer addSubview:_loadingProgressView positioned:NSWindowBelow relativeTo:nil];

    // Security icon (lock/warning) inside container
    CGFloat iconSize = 16;
    CGFloat iconX = 10;
    CGFloat iconY = (containerHeight - iconSize) / 2;
    _securityIcon = [[NSImageView alloc] initWithFrame:NSMakeRect(iconX, iconY, iconSize, iconSize)];
    _securityIcon.imageScaling = NSImageScaleProportionallyUpOrDown;
    _securityIcon.hidden = YES;  // Hidden until we have a URL
    [_urlContainer addSubview:_securityIcon];

    // URL text field inside container - vertically centered, shifted down slightly
    CGFloat textFieldInset = 10;  // Increased right padding
    CGFloat textFieldLeftInset = iconX + iconSize + 8;  // After security icon, with more padding
    CGFloat textFieldHeight = 20;
    CGFloat textFieldY = (containerHeight - textFieldHeight) / 2 - 2;  // Moved down 2 pixels
    _urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(textFieldLeftInset, textFieldY, containerWidth - textFieldLeftInset - textFieldInset, textFieldHeight)];
    _urlField.bezeled = NO;
    _urlField.drawsBackground = NO;
    _urlField.backgroundColor = [NSColor clearColor];
    _urlField.textColor = TextColor();
    _urlField.font = [NSFont systemFontOfSize:13];
    _urlField.focusRingType = NSFocusRingTypeNone;
    _urlField.delegate = self;
    _urlField.placeholderString = @"Search or enter URL";
    _urlField.autoresizingMask = NSViewWidthSizable;

    NSTextFieldCell* cell = _urlField.cell;
    cell.truncatesLastVisibleLine = YES;
    cell.lineBreakMode = NSLineBreakByTruncatingTail;
    cell.wraps = NO;
    cell.scrollable = YES;

    [_urlContainer addSubview:_urlField];
}

- (NSButton*)createNavButton:(NSString*)symbolName frame:(NSRect)frame {
    NSButton* btn = [[NSButton alloc] initWithFrame:frame];
    btn.bezelStyle = NSBezelStyleInline;
    btn.bordered = NO;
    btn.image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:symbolName];
    btn.contentTintColor = SecondaryTextColor();
    btn.target = self;
    return btn;
}

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

- (void)setURL:(NSString*)url {
    _urlField.stringValue = url ?: @"";

    // Update security indicator
    if (!url || url.length == 0) {
        _securityIcon.hidden = YES;
    } else if ([url hasPrefix:@"https://"]) {
        // Secure connection - show lock icon
        _securityIcon.image = [NSImage imageWithSystemSymbolName:@"lock.fill"
                                      accessibilityDescription:@"Secure"];
        _securityIcon.contentTintColor = [NSColor colorWithRed:0.3 green:0.7 blue:0.4 alpha:1.0];
        _securityIcon.hidden = NO;
    } else if ([url hasPrefix:@"http://"]) {
        // Insecure connection - show warning icon
        _securityIcon.image = [NSImage imageWithSystemSymbolName:@"exclamationmark.triangle.fill"
                                      accessibilityDescription:@"Not Secure"];
        _securityIcon.contentTintColor = [NSColor colorWithRed:0.9 green:0.6 blue:0.2 alpha:1.0];
        _securityIcon.hidden = NO;
    } else {
        // Other protocols (file://, data://, etc.)
        _securityIcon.hidden = YES;
    }
}

- (void)setCanGoBack:(BOOL)canGoBack canGoForward:(BOOL)canGoForward {
    _backButton.enabled = canGoBack;
    _forwardButton.enabled = canGoForward;
    _backButton.contentTintColor = canGoBack ? SecondaryTextColor() : DisabledColor();
    _forwardButton.contentTintColor = canGoForward ? SecondaryTextColor() : DisabledColor();
}

- (void)setLoading:(BOOL)isLoading {
    if (isLoading && !_isLoading) {
        // Start loading animation
        _isLoading = YES;
        _loadingProgress = 0.0;
        _loadingProgressView.hidden = NO;

        // Animate progress from 0 to ~90% quickly, then slow down
        [_loadingAnimationTimer invalidate];
        _loadingAnimationTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                                 repeats:YES
                                                                   block:^(NSTimer* timer) {
            if (self->_loadingProgress < 0.9) {
                // Fast progress to 90%
                self->_loadingProgress += 0.03;
            } else if (self->_loadingProgress < 0.98) {
                // Slow progress after 90%
                self->_loadingProgress += 0.002;
            }
            [self updateLoadingProgressView];
        }];
    } else if (!isLoading && _isLoading) {
        // Complete loading animation
        _isLoading = NO;
        [_loadingAnimationTimer invalidate];
        _loadingAnimationTimer = nil;

        // Animate to 100% then fade out
        _loadingProgress = 1.0;
        [self updateLoadingProgressView];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
                context.duration = 0.3;
                self->_loadingProgressView.animator.alphaValue = 0.0;
            } completionHandler:^{
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
    [self.window makeFirstResponder:_urlField];
    [_urlField selectText:nil];
}

#pragma mark - NSTextFieldDelegate

- (void)controlTextDidEndEditing:(NSNotification*)notification {
    NSTextField* textField = notification.object;
    if (textField == _urlField) {
        NSString* url = _urlField.stringValue;
        if (url.length > 0) {
            [_windowController navigateToURL:url];
        }
    }
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector {
    (void)control;
    (void)textView;
    if (commandSelector == @selector(insertNewline:)) {
        NSString* url = _urlField.stringValue;
        if (url.length > 0) {
            [_windowController navigateToURL:url];
        }
        // Resign first responder to blur the URL field
        [self.window makeFirstResponder:nil];
        return YES;
    }
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [ToolbarBackgroundColor() setFill];
    NSRectFill(self.bounds);

    // Draw separator line at top
    [[NSColor colorWithRed:0.2 green:0.2 blue:0.22 alpha:1.0] setFill];
    NSRectFill(NSMakeRect(0, self.bounds.size.height - 1, self.bounds.size.width, 1));
}

@end
