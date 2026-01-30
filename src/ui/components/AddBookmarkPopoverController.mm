#import "AddBookmarkPopoverController.h"
#import "SidebarView.h"
#import "Components.h"
#include "bookmark_storage.h"

@implementation AddBookmarkPopoverController {
    NSPopover* _popover;
    NSPanel* _panel;  // For sheet mode
    NSTextField* _urlField;
    NSTextField* _titleField;
    NSTextView* _descriptionField;
    NSPopUpButton* _folderPicker;
    NSImageView* _bookmarkIcon;
    NSButton* _actionButton;  // Add or Save button
    BookmarkPopoverMode _mode;
    int64_t _editBookmarkId;
}

@synthesize mode = _mode;

- (void)loadView {
    [self loadViewWithMode:BookmarkPopoverModeAdd];
}

- (void)loadViewWithMode:(BookmarkPopoverMode)mode {
    _mode = mode;

    CGFloat width = 340;
    CGFloat height = 290;

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

    // URL field (Address)
    y -= fieldHeight + 4;
    CGFloat fieldWidth = width - padding * 2 - iconSize - [DSSpacing sm];
    _urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, fieldWidth, fieldHeight)];
    _urlField.font = [DSTypography fontWithStyle:DSFontStyleBody];
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

    // Nickname field (Title)
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

    // Buttons - Cancel on left, Add/Save on right
    y -= 44;
    CGFloat buttonWidth = 80;
    CGFloat buttonHeight = 28;

    NSButton* cancelBtn = [[NSButton alloc] initWithFrame:NSMakeRect(padding, y, buttonWidth, buttonHeight)];
    cancelBtn.title = @"Cancel";
    cancelBtn.bezelStyle = NSBezelStyleRounded;
    cancelBtn.target = self;
    cancelBtn.action = @selector(cancelClicked:);
    [contentView addSubview:cancelBtn];

    _actionButton = [[NSButton alloc] initWithFrame:NSMakeRect(width - padding - buttonWidth, y, buttonWidth, buttonHeight)];
    _actionButton.title = (mode == BookmarkPopoverModeAdd) ? @"Add" : @"Save";
    _actionButton.bezelStyle = NSBezelStyleRounded;
    _actionButton.keyEquivalent = @"\r";
    _actionButton.target = self;
    _actionButton.action = @selector(actionClicked:);
    [contentView addSubview:_actionButton];

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

#pragma mark - Public Methods

- (void)showRelativeToView:(NSView*)view withUrl:(NSString*)url title:(NSString*)title {
    [self loadViewWithMode:BookmarkPopoverModeAdd];
    [self setupFieldsWithUrl:url title:title folder:nil];

    if (!_popover) {
        _popover = [[NSPopover alloc] init];
        _popover.contentViewController = self;
        _popover.behavior = NSPopoverBehaviorTransient;
    }

    [_popover showRelativeToRect:view.bounds
                          ofView:view
                   preferredEdge:NSRectEdgeMinY];

    [_popover.contentViewController.view.window makeFirstResponder:_urlField];
}

- (void)showEditRelativeToView:(NSView*)view bookmarkId:(int64_t)bookmarkId {
    [self loadViewWithMode:BookmarkPopoverModeEdit];
    _editBookmarkId = bookmarkId;

    BookmarkStorage* storage = GetBookmarkStorage();
    if (storage) {
        auto allBookmarks = storage->GetAllBookmarks();
        for (const auto& bm : allBookmarks) {
            if (bm.id == bookmarkId) {
                NSString* url = [NSString stringWithUTF8String:bm.url.c_str()];
                NSString* title = [NSString stringWithUTF8String:bm.title.c_str()];
                NSString* folder = [NSString stringWithUTF8String:bm.folder.c_str()];
                [self setupFieldsWithUrl:url title:title folder:folder];
                break;
            }
        }
    }

    if (!_popover) {
        _popover = [[NSPopover alloc] init];
        _popover.contentViewController = self;
        _popover.behavior = NSPopoverBehaviorTransient;
    }

    [_popover showRelativeToRect:view.bounds
                          ofView:view
                   preferredEdge:NSRectEdgeMinY];

    [_popover.contentViewController.view.window makeFirstResponder:_titleField];
}

- (void)showAsSheetInWindow:(NSWindow*)window withUrl:(NSString*)url title:(NSString*)title {
    [self loadViewWithMode:BookmarkPopoverModeAdd];
    [self setupFieldsWithUrl:url title:title folder:nil];

    if (!_panel) {
        _panel = [[NSPanel alloc] initWithContentRect:self.view.bounds
                                            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                              backing:NSBackingStoreBuffered
                                                defer:YES];
        _panel.contentView = self.view;
        _panel.title = @"Add Bookmark";
    }

    [window beginSheet:_panel completionHandler:nil];
    [_panel makeFirstResponder:_urlField];
}

- (void)showEditAsSheetInWindow:(NSWindow*)window bookmarkId:(int64_t)bookmarkId {
    [self loadViewWithMode:BookmarkPopoverModeEdit];
    _editBookmarkId = bookmarkId;

    BookmarkStorage* storage = GetBookmarkStorage();
    if (storage) {
        auto allBookmarks = storage->GetAllBookmarks();
        for (const auto& bm : allBookmarks) {
            if (bm.id == bookmarkId) {
                NSString* url = [NSString stringWithUTF8String:bm.url.c_str()];
                NSString* title = [NSString stringWithUTF8String:bm.title.c_str()];
                NSString* folder = [NSString stringWithUTF8String:bm.folder.c_str()];
                [self setupFieldsWithUrl:url title:title folder:folder];
                break;
            }
        }
    }

    if (!_panel) {
        _panel = [[NSPanel alloc] initWithContentRect:self.view.bounds
                                            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                              backing:NSBackingStoreBuffered
                                                defer:YES];
        _panel.contentView = self.view;
    }
    _panel.title = @"Edit Bookmark";

    [window beginSheet:_panel completionHandler:nil];
    [_panel makeFirstResponder:_titleField];
}

- (void)setupFieldsWithUrl:(NSString*)url title:(NSString*)title folder:(NSString*)folder {
    _urlField.stringValue = url ?: @"";
    _titleField.stringValue = title ?: @"";
    _descriptionField.string = @"";

    [self populateFolderPicker];
    if (folder && folder.length > 0) {
        [_folderPicker selectItemWithTitle:folder];
    } else {
        [_folderPicker selectItemAtIndex:0];
    }
}

- (void)close {
    if (_popover) {
        [_popover close];
    }
    if (_panel && _panel.sheetParent) {
        [_panel.sheetParent endSheet:_panel];
    }
}

#pragma mark - Actions

- (void)cancelClicked:(id)sender {
    (void)sender;
    [self close];
}

- (void)actionClicked:(id)sender {
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

    NSString* folder = @"";
    NSInteger selectedIndex = [_folderPicker indexOfSelectedItem];
    if (selectedIndex > 0) {
        NSMenuItem* selectedItem = [_folderPicker selectedItem];
        if (!selectedItem.isSeparatorItem) {
            folder = selectedItem.title;
        }
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        if (_mode == BookmarkPopoverModeAdd) {
            (void)bookmarks->AddBookmark([url UTF8String], [title UTF8String], [folder UTF8String]);
        } else {
            bookmarks->UpdateBookmark(_editBookmarkId, [title UTF8String], [folder UTF8String]);
        }

        if (_sidebarView) {
            [_sidebarView reloadBookmarks];
        }
    }

    [self close];
}

@end
