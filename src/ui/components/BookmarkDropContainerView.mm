#import "BookmarkDropContainerView.h"
#import "SidebarView.h"
#import "Components.h"
#include "bookmark_storage.h"

// Pasteboard type for bookmark drag & drop
NSString* const kBookmarkPasteboardType = @"com.orbfox.bookmark";

@implementation BookmarkDropContainerView

- (BOOL)isFlipped { return YES; }

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[kBookmarkPasteboardType]];

        // Create drop indicator (2px accent-colored line)
        _dropIndicator = [[NSView alloc] initWithFrame:NSZeroRect];
        _dropIndicator.wantsLayer = YES;
        _dropIndicator.layer.backgroundColor = [DSColors accent].CGColor;
        _dropIndicator.layer.cornerRadius = 1;
        _dropIndicator.hidden = YES;
        [self addSubview:_dropIndicator];

        _dropPosition = 0;
    }
    return self;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    (void)sender;
    return NSDragOperationMove;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint location = [self convertPoint:[sender draggingLocation] fromView:nil];

    // Reset highlight
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropOnFolderHeader = NO;
    _dropTargetFolder = nil;
    _dropPosition = 0;

    CGFloat indicatorY = 0;
    NSString* currentFolder = nil;  // Tracks current folder context
    int positionInFolder = 0;       // Position counter within current folder
    BOOL foundDropPoint = NO;

    // Sort subviews by Y position for proper iteration
    NSArray* sortedSubviews = [self.subviews sortedArrayUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        if (a == self->_dropIndicator) return NSOrderedDescending;
        if (b == self->_dropIndicator) return NSOrderedAscending;
        return [@(a.frame.origin.y) compare:@(b.frame.origin.y)];
    }];

    NSView* lastView = nil;
    for (NSView* subview in sortedSubviews) {
        if (subview == _dropIndicator) continue;

        NSRect frame = subview.frame;

        // Check if this is a folder header (not a DSRow and height < 40)
        BOOL isFolderHeader = (![subview isKindOfClass:[DSRow class]] && frame.size.height < 40);

        if (isFolderHeader) {
            // Get folder name from header
            NSString* folderName = nil;
            for (NSView* headerSubview in subview.subviews) {
                if ([headerSubview isKindOfClass:[NSTextField class]]) {
                    NSTextField* label = (NSTextField*)headerSubview;
                    if (label.frame.origin.x > 30) {  // Folder name label (not chevron area)
                        folderName = label.stringValue;
                        break;
                    }
                }
            }

            // Check if dropping ON this folder header
            if (location.y >= frame.origin.y && location.y < frame.origin.y + frame.size.height) {
                // Highlight the folder header
                subview.wantsLayer = YES;
                subview.layer.backgroundColor = [DSColors surfaceHover].CGColor;
                _highlightedFolderHeader = subview;
                _dropOnFolderHeader = YES;
                _dropTargetFolder = folderName;
                _dropPosition = 0;  // First position in folder
                _dropIndicator.hidden = YES;
                foundDropPoint = YES;
                break;
            }

            // If mouse is before this folder header, drop at end of previous section
            if (location.y < frame.origin.y && !foundDropPoint) {
                _dropTargetFolder = currentFolder;
                _dropPosition = positionInFolder;
                indicatorY = frame.origin.y - 1;
                foundDropPoint = YES;
                break;
            }

            // Update current folder context
            currentFolder = folderName;
            positionInFolder = 0;

        } else if ([subview isKindOfClass:[DSRow class]]) {
            // This is a bookmark row
            CGFloat midY = frame.origin.y + frame.size.height / 2;

            if (location.y < midY && !foundDropPoint) {
                // Drop before this bookmark
                _dropTargetFolder = currentFolder;
                _dropPosition = positionInFolder;
                indicatorY = frame.origin.y - 1;
                foundDropPoint = YES;
                break;
            }

            positionInFolder++;
        }

        lastView = subview;
    }

    // If past all items, drop at end
    if (!foundDropPoint && lastView) {
        _dropTargetFolder = currentFolder;
        _dropPosition = positionInFolder;
        indicatorY = lastView.frame.origin.y + lastView.frame.size.height + 1;
    }

    // Show drop indicator (unless dropping ON a folder)
    if (!_dropOnFolderHeader) {
        CGFloat padding = [DSSpacing xs];
        CGFloat indent = (_dropTargetFolder.length > 0) ? [DSSpacing md] : 0;
        _dropIndicator.frame = NSMakeRect(padding + indent, indicatorY, self.bounds.size.width - padding * 2 - indent, 2);
        _dropIndicator.hidden = NO;
    }

    return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropTargetFolder = nil;
    _dropPosition = 0;
    _dropOnFolderHeader = NO;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }

    NSPasteboard* pboard = [sender draggingPasteboard];
    NSData* data = [pboard dataForType:kBookmarkPasteboardType];
    if (!data) return NO;

    NSDictionary* dragData = [NSPropertyListSerialization propertyListWithData:data
                                                                       options:NSPropertyListImmutable
                                                                        format:nil
                                                                         error:nil];
    if (!dragData) return NO;

    int64_t bookmarkId = [dragData[@"id"] longLongValue];
    NSString* sourceFolder = dragData[@"folder"];
    (void)sourceFolder;  // Currently unused, but available for future use

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return NO;

    // Determine target folder
    NSString* targetFolder = _dropTargetFolder ?: @"";

    // Adjust position if moving within same folder (need to account for removed item)
    int finalPosition = _dropPosition;
    if ([sourceFolder isEqualToString:targetFolder] || (sourceFolder.length == 0 && targetFolder.length == 0)) {
        // Moving within same folder - check if source is before drop position
        std::vector<Bookmark> folderBookmarks;
        if (targetFolder.length > 0) {
            folderBookmarks = bookmarks->GetBookmarksInFolder([targetFolder UTF8String]);
        } else {
            std::vector<Bookmark> all = bookmarks->GetAllBookmarks();
            for (const auto& bm : all) {
                if (bm.folder.empty()) {
                    folderBookmarks.push_back(bm);
                }
            }
        }

        int sourcePosition = -1;
        for (size_t i = 0; i < folderBookmarks.size(); i++) {
            if (folderBookmarks[i].id == bookmarkId) {
                sourcePosition = (int)i;
                break;
            }
        }

        if (sourcePosition >= 0 && sourcePosition < finalPosition) {
            finalPosition--;  // Account for removal
        }
    }

    // Move the bookmark
    bookmarks->MoveBookmark(bookmarkId, [targetFolder UTF8String], finalPosition);

    // Reload bookmarks in sidebar
    if (_sidebarView) {
        [_sidebarView reloadBookmarks];
    }

    return YES;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    _dropIndicator.hidden = YES;
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropTargetFolder = nil;
    _dropPosition = 0;
    _dropOnFolderHeader = NO;
}

@end
