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

@implementation ToolbarView

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
    CGFloat containerPadding = 5;
    CGFloat containerHeight = self.bounds.size.height - (containerPadding * 2);
    CGFloat containerWidth = self.bounds.size.width - x - 12;

    NSView* urlContainer = [[NSView alloc] initWithFrame:NSMakeRect(x, containerPadding, containerWidth, containerHeight)];
    urlContainer.wantsLayer = YES;
    urlContainer.layer.backgroundColor = URLFieldBackgroundColor().CGColor;
    urlContainer.layer.cornerRadius = 6;
    urlContainer.autoresizingMask = NSViewWidthSizable;
    [self addSubview:urlContainer];

    // URL text field inside container - vertically centered
    CGFloat textFieldInset = 8;
    CGFloat textFieldHeight = 20;
    CGFloat textFieldY = (containerHeight - textFieldHeight) / 2;
    _urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(textFieldInset, textFieldY, containerWidth - (textFieldInset * 2), textFieldHeight)];
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

    [urlContainer addSubview:_urlField];
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
}

- (void)setCanGoBack:(BOOL)canGoBack canGoForward:(BOOL)canGoForward {
    _backButton.enabled = canGoBack;
    _forwardButton.enabled = canGoForward;
    _backButton.contentTintColor = canGoBack ? SecondaryTextColor() : DisabledColor();
    _forwardButton.contentTintColor = canGoForward ? SecondaryTextColor() : DisabledColor();
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
