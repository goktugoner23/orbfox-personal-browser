#import "BookmarkEditPopover.h"
#import "MainWindowController.h"
#import "components/Components.h"
#include "bookmark_storage.h"

extern BookmarkStorage* GetBookmarkStorage();

@implementation BookmarkEditPopoverController {
    NSPopover* _popover;
    NSTextField* _nameField;
    NSPopUpButton* _folderPicker;
    int64_t _bookmarkId;
    NSString* _originalUrl;
}

- (void)loadView {
    // Create popover content view
    CGFloat width = 280;
    CGFloat height = 150;

    NSView* contentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    contentView.wantsLayer = YES;

    CGFloat y = height - 28;
    CGFloat padding = [DSSpacing md];
    CGFloat labelWidth = 45;
    CGFloat fieldX = padding + labelWidth + 4;
    CGFloat fieldWidth = width - fieldX - padding;

    // Name label
    NSTextField* nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, labelWidth, 20)];
    nameLabel.stringValue = @"Name";
    nameLabel.font = [DSTypography fontWithStyle:DSFontStyleCaptionMedium];
    nameLabel.textColor = [DSColors textSecondary];
    nameLabel.bezeled = NO;
    nameLabel.drawsBackground = NO;
    nameLabel.editable = NO;
    [contentView addSubview:nameLabel];

    // Name text field
    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(fieldX, y - 2, fieldWidth, 24)];
    _nameField.font = [DSTypography fontWithStyle:DSFontStyleBody];
    _nameField.bezelStyle = NSTextFieldRoundedBezel;
    [contentView addSubview:_nameField];

    y -= 36;

    // Folder label
    NSTextField* folderLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, labelWidth, 20)];
    folderLabel.stringValue = @"Folder";
    folderLabel.font = [DSTypography fontWithStyle:DSFontStyleCaptionMedium];
    folderLabel.textColor = [DSColors textSecondary];
    folderLabel.bezeled = NO;
    folderLabel.drawsBackground = NO;
    folderLabel.editable = NO;
    [contentView addSubview:folderLabel];

    // Folder picker
    _folderPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(fieldX, y - 3, fieldWidth, 26) pullsDown:NO];
    [self populateFolderPicker];
    [contentView addSubview:_folderPicker];

    y -= 48;

    // Buttons
    NSButton* removeBtn = [[NSButton alloc] initWithFrame:NSMakeRect(padding, y, 70, 28)];
    removeBtn.title = @"Remove";
    removeBtn.bezelStyle = NSBezelStyleRounded;
    removeBtn.target = self;
    removeBtn.action = @selector(removeClicked:);
    [contentView addSubview:removeBtn];

    NSButton* doneBtn = [[NSButton alloc] initWithFrame:NSMakeRect(width - padding - 60, y, 60, 28)];
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

    _nameField.stringValue = title ?: @"";

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

    // Focus the name field
    [_popover.contentViewController.view.window makeFirstResponder:_nameField];
}

- (void)close {
    [_popover close];
}

- (void)doneClicked:(id)sender {
    (void)sender;

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks && _bookmarkId > 0) {
        NSString* newTitle = _nameField.stringValue;
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
