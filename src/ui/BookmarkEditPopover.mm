#import "BookmarkEditPopover.h"
#import "MainWindowController.h"
#import "components/Components.h"
#include "bookmark_storage.h"

@implementation BookmarkEditPopoverController {
    NSPopover* _popover;
    NSTextField* _urlField;
    NSTextField* _nicknameField;
    NSTextView* _descriptionField;
    NSPopUpButton* _folderPicker;
    int64_t _bookmarkId;
    NSString* _originalUrl;
}

- (void)loadView {
    // Create popover content view - match AddBookmarkPopoverController layout
    CGFloat width = 340;
    CGFloat height = 290;

    NSView* contentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    contentView.wantsLayer = YES;

    CGFloat padding = [DSSpacing md];
    CGFloat labelHeight = 16;
    CGFloat fieldHeight = 28;
    CGFloat y = height - padding;

    // URL label
    y -= labelHeight;
    NSTextField* urlLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, 60, labelHeight)];
    urlLabel.stringValue = @"Address";
    urlLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    urlLabel.textColor = [DSColors textSecondary];
    urlLabel.bezeled = NO;
    urlLabel.drawsBackground = NO;
    urlLabel.editable = NO;
    [contentView addSubview:urlLabel];

    // URL field
    y -= fieldHeight + 4;
    _urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, fieldHeight)];
    _urlField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _urlField.bezelStyle = NSTextFieldRoundedBezel;
    _urlField.placeholderString = @"https://";
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
    _nicknameField = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, fieldHeight)];
    _nicknameField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _nicknameField.bezelStyle = NSTextFieldRoundedBezel;
    _nicknameField.placeholderString = @"Bookmark name";
    [contentView addSubview:_nicknameField];

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

    // Buttons - Remove on left, Done on right
    y -= 44;
    CGFloat buttonWidth = 80;
    CGFloat buttonHeight = 28;

    NSButton* removeBtn = [[NSButton alloc] initWithFrame:NSMakeRect(padding, y, buttonWidth, buttonHeight)];
    removeBtn.title = @"Remove";
    removeBtn.bezelStyle = NSBezelStyleRounded;
    removeBtn.target = self;
    removeBtn.action = @selector(removeClicked:);
    [contentView addSubview:removeBtn];

    NSButton* doneBtn = [[NSButton alloc] initWithFrame:NSMakeRect(width - padding - buttonWidth, y, buttonWidth, buttonHeight)];
    doneBtn.title = @"Done";
    doneBtn.bezelStyle = NSBezelStyleRounded;
    doneBtn.keyEquivalent = @"\r";
    doneBtn.target = self;
    doneBtn.action = @selector(doneClicked:);
    [contentView addSubview:doneBtn];

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

- (void)showRelativeToView:(NSView*)view
               forBookmark:(int64_t)bookmarkId
                     title:(NSString*)title
                       url:(NSString*)url
                    folder:(NSString*)folder {
    _bookmarkId = bookmarkId;
    _originalUrl = url;

    if (!self.view) {
        [self loadView];
    }

    _urlField.stringValue = url ?: @"";
    _nicknameField.stringValue = title ?: @"";
    _descriptionField.string = @"";

    [self populateFolderPicker];
    if (folder && folder.length > 0) {
        [_folderPicker selectItemWithTitle:folder];
    } else {
        [_folderPicker selectItemAtIndex:0];
    }

    if (!_popover) {
        _popover = [[NSPopover alloc] init];
        _popover.contentViewController = self;
        _popover.behavior = NSPopoverBehaviorTransient;
    }

    [_popover showRelativeToRect:view.bounds
                          ofView:view
                   preferredEdge:NSRectEdgeMinY];

    // Focus the nickname field
    [_popover.contentViewController.view.window makeFirstResponder:_nicknameField];
}

- (void)close {
    [_popover close];
}

- (void)doneClicked:(id)sender {
    (void)sender;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks && _bookmarkId > 0) {
        NSString* newTitle = _nicknameField.stringValue;
        NSString* newFolder = @"";

        NSInteger selectedIndex = [_folderPicker indexOfSelectedItem];
        if (selectedIndex > 0) {
            // Check if it's a separator (skip it)
            NSMenuItem* selectedItem = [_folderPicker selectedItem];
            if (!selectedItem.isSeparatorItem) {
                newFolder = selectedItem.title;
            }
        }

        bookmarks->UpdateBookmark(_bookmarkId, [newTitle UTF8String], [newFolder UTF8String]);

        MainWindowController* wc = self.windowController;
        if (wc) {
            [wc reloadBookmarksPanel];
            [wc updateBookmarkState];
        }
    }

    [_popover close];
    if (_onDismiss) _onDismiss();
}

- (void)removeClicked:(id)sender {
    (void)sender;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks && _bookmarkId > 0) {
        bookmarks->DeleteBookmark(_bookmarkId);
        MainWindowController* wc = self.windowController;
        if (wc) {
            [wc updateBookmarkState];
            [wc reloadBookmarksPanel];
        }
    }

    [_popover close];
    if (_onDismiss) _onDismiss();
}

@end
