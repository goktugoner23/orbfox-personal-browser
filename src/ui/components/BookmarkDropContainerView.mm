#import "BookmarkDropContainerView.h"
#import "SidebarView.h"
#import "SidebarHelperViews.h"
#import "Components.h"
#include "bookmark_storage.h"

// Pasteboard types for bookmark drag & drop
NSString* const kBookmarkPasteboardType = @"com.orbfox.bookmark";
NSString* const kBookmarkFolderPasteboardType = @"com.orbfox.bookmark.folder";

@implementation BookmarkDropContainerView

- (BOOL)isFlipped { return YES; }

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[kBookmarkPasteboardType, kBookmarkFolderPasteboardType]];

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
    NSPasteboard* pboard = [sender draggingPasteboard];
    if ([pboard dataForType:kBookmarkPasteboardType] || [pboard dataForType:kBookmarkFolderPasteboardType]) {
        return NSDragOperationMove;
    }
    return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint location = [self convertPoint:[sender draggingLocation] fromView:nil];
    NSPasteboard* pboard = [sender draggingPasteboard];
    BOOL isDraggingFolder = [pboard dataForType:kBookmarkFolderPasteboardType] != nil;

    // Reset highlight
    if (_highlightedFolderHeader) {
        _highlightedFolderHeader.layer.backgroundColor = nil;
        _highlightedFolderHeader = nil;
    }
    _dropOnFolderHeader = NO;
    _dropTargetFolder = nil;
    _dropPosition = 0;

    CGFloat indicatorY = 0;
    int rootPosition = 0;           // Unified position counter at root level
    BOOL foundDropPoint = NO;

    // Threshold for indent detection (items inside folders are indented)
    CGFloat indentThreshold = 15.0;

    // Sort subviews by Y position for proper iteration
    NSArray* sortedSubviews = [self.subviews sortedArrayUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        if (a == self->_dropIndicator) return NSOrderedDescending;
        if (b == self->_dropIndicator) return NSOrderedAscending;
        return [@(a.frame.origin.y) compare:@(b.frame.origin.y)];
    }];

    NSView* lastRootView = nil;
    NSView* lastVisibleView = nil;  // Track last visible view for indicator positioning
    NSString* lastFolderName = nil;
    int positionInLastFolder = 0;

    for (NSView* subview in sortedSubviews) {
        if (subview == _dropIndicator) continue;

        NSRect frame = subview.frame;

        // Check if this item is indented (inside a folder)
        BOOL isIndented = (frame.origin.x > indentThreshold);

        // Check if this is a folder header
        BOOL isFolderHeader = ([subview isKindOfClass:[DraggableFolderHeaderView class]] ||
                               (![subview isKindOfClass:[DSRow class]] && frame.size.height < 40 && frame.size.height > 0));

        if (isFolderHeader) {
            // Get folder name from header
            NSString* folderName = nil;
            if ([subview isKindOfClass:[DraggableFolderHeaderView class]]) {
                folderName = ((DraggableFolderHeaderView*)subview).folderName;
            } else {
                for (NSView* headerSubview in subview.subviews) {
                    if ([headerSubview isKindOfClass:[NSTextField class]]) {
                        NSTextField* label = (NSTextField*)headerSubview;
                        if (label.frame.origin.x > 30) {
                            folderName = label.stringValue;
                            break;
                        }
                    }
                }
            }

            // Check if mouse is before this folder (drop at root before it)
            if (location.y < frame.origin.y && !foundDropPoint) {
                _dropTargetFolder = nil;  // Root level
                _dropPosition = rootPosition;
                indicatorY = frame.origin.y - 1;
                foundDropPoint = YES;
                break;
            }

            // Check if mouse is ON this folder header
            if (location.y >= frame.origin.y && location.y < frame.origin.y + frame.size.height) {
                if (!isDraggingFolder) {
                    // For bookmarks: check if in the middle 60% of the header = drop INTO folder
                    CGFloat headerTop = frame.origin.y;
                    CGFloat headerBottom = frame.origin.y + frame.size.height;
                    CGFloat dropZoneTop = headerTop + frame.size.height * 0.2;
                    CGFloat dropZoneBottom = headerBottom - frame.size.height * 0.2;

                    if (location.y >= dropZoneTop && location.y < dropZoneBottom) {
                        // Drop INTO folder
                        subview.wantsLayer = YES;
                        subview.layer.backgroundColor = [DSColors surfaceHover].CGColor;
                        _highlightedFolderHeader = subview;
                        _dropOnFolderHeader = YES;
                        _dropTargetFolder = folderName;
                        _dropPosition = 0;
                        _dropIndicator.hidden = YES;
                        foundDropPoint = YES;
                        break;
                    } else if (location.y < dropZoneTop) {
                        // Top edge - drop before folder at root
                        _dropTargetFolder = nil;
                        _dropPosition = rootPosition;
                        indicatorY = frame.origin.y - 1;
                        foundDropPoint = YES;
                        break;
                    }
                    // Bottom edge - continue to next item
                } else {
                    // For folders: use midpoint for before/after
                    CGFloat midY = frame.origin.y + frame.size.height / 2;
                    if (location.y < midY) {
                        _dropTargetFolder = nil;
                        _dropPosition = rootPosition;
                        indicatorY = frame.origin.y - 1;
                        foundDropPoint = YES;
                        break;
                    }
                    // Bottom half - will be handled by next item or end-of-list
                }
            }

            // Update tracking
            lastFolderName = folderName;
            positionInLastFolder = 0;
            lastRootView = subview;
            lastVisibleView = subview;
            rootPosition++;

        } else if ([subview isKindOfClass:[DSRow class]]) {
            // This is a bookmark row
            CGFloat midY = frame.origin.y + frame.size.height / 2;

            if (isIndented) {
                // Bookmark inside a folder
                if (location.y < frame.origin.y && !foundDropPoint) {
                    // Before this indented item - could be after folder header or between folder items
                    if (!isDraggingFolder) {
                        _dropTargetFolder = lastFolderName;
                        _dropPosition = positionInLastFolder;
                        indicatorY = frame.origin.y - 1;
                        foundDropPoint = YES;
                        break;
                    }
                }

                if (location.y >= frame.origin.y && location.y < frame.origin.y + frame.size.height && !foundDropPoint) {
                    if (!isDraggingFolder) {
                        if (location.y < midY) {
                            _dropTargetFolder = lastFolderName;
                            _dropPosition = positionInLastFolder;
                            indicatorY = frame.origin.y - 1;
                        } else {
                            _dropTargetFolder = lastFolderName;
                            _dropPosition = positionInLastFolder + 1;
                            indicatorY = frame.origin.y + frame.size.height + 1;
                        }
                        foundDropPoint = YES;
                        break;
                    }
                }
                lastVisibleView = subview;
                positionInLastFolder++;
            } else {
                // Root-level bookmark
                if (location.y < frame.origin.y && !foundDropPoint) {
                    _dropTargetFolder = nil;
                    _dropPosition = rootPosition;
                    indicatorY = frame.origin.y - 1;
                    foundDropPoint = YES;
                    break;
                }

                if (location.y >= frame.origin.y && location.y < frame.origin.y + frame.size.height && !foundDropPoint) {
                    if (location.y < midY) {
                        _dropTargetFolder = nil;
                        _dropPosition = rootPosition;
                        indicatorY = frame.origin.y - 1;
                    } else {
                        _dropTargetFolder = nil;
                        _dropPosition = rootPosition + 1;
                        indicatorY = frame.origin.y + frame.size.height + 1;
                    }
                    foundDropPoint = YES;
                    break;
                }

                lastRootView = subview;
                lastVisibleView = subview;
                lastFolderName = nil;  // Reset - we're back at root
                rootPosition++;
            }
        }
    }

    // If past all items, drop at end
    if (!foundDropPoint) {
        _dropTargetFolder = nil;  // Always root level at the end
        _dropPosition = rootPosition;
        // Use lastVisibleView for indicator to show at actual visual bottom
        if (lastVisibleView) {
            indicatorY = lastVisibleView.frame.origin.y + lastVisibleView.frame.size.height + 1;
        } else if (lastRootView) {
            indicatorY = lastRootView.frame.origin.y + lastRootView.frame.size.height + 1;
        }
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
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return NO;

    // Check if this is a folder drag
    NSData* folderData = [pboard dataForType:kBookmarkFolderPasteboardType];
    if (folderData) {
        NSDictionary* dragData = [NSPropertyListSerialization propertyListWithData:folderData
                                                                           options:NSPropertyListImmutable
                                                                            format:nil
                                                                             error:nil];
        if (!dragData) return NO;

        NSString* folderName = dragData[@"name"];
        if (!folderName) return NO;

        // Get current folder position for adjustment
        int sourcePosition = bookmarks->GetFolderPosition([folderName UTF8String]);

        int finalPosition = _dropPosition;
        if (sourcePosition >= 0 && sourcePosition < finalPosition) {
            finalPosition--;  // Account for removal
        }

        // Use MoveFolderAtRoot for unified ordering
        bookmarks->MoveFolderAtRoot([folderName UTF8String], finalPosition);

        if (_sidebarView) {
            [_sidebarView reloadBookmarks];
        }
        return YES;
    }

    // Handle bookmark drag
    NSData* data = [pboard dataForType:kBookmarkPasteboardType];
    if (!data) return NO;

    NSDictionary* dragData = [NSPropertyListSerialization propertyListWithData:data
                                                                       options:NSPropertyListImmutable
                                                                        format:nil
                                                                         error:nil];
    if (!dragData) return NO;

    int64_t bookmarkId = [dragData[@"id"] longLongValue];
    NSString* sourceFolder = dragData[@"folder"];

    // Determine target folder (nil means root level)
    NSString* targetFolder = _dropTargetFolder;
    BOOL movingToRoot = (targetFolder == nil || targetFolder.length == 0);
    BOOL movingFromRoot = (sourceFolder == nil || sourceFolder.length == 0);

    int finalPosition = _dropPosition;

    if (movingToRoot && movingFromRoot) {
        // Moving within root level - use unified positioning
        // Get source bookmark's current position
        std::vector<Bookmark> all = bookmarks->GetAllBookmarks();
        int sourcePosition = -1;
        for (const auto& bm : all) {
            if (bm.id == bookmarkId && bm.folder.empty()) {
                sourcePosition = bm.position;
                break;
            }
        }

        if (sourcePosition >= 0 && sourcePosition < finalPosition) {
            finalPosition--;  // Account for removal
        }

        bookmarks->MoveBookmarkAtRoot(bookmarkId, finalPosition);
    } else if (!movingToRoot) {
        // Moving into a folder - use folder's internal positioning
        if ([sourceFolder isEqualToString:targetFolder]) {
            // Moving within same folder
            std::vector<Bookmark> folderBookmarks = bookmarks->GetBookmarksInFolder([targetFolder UTF8String]);
            int sourcePosition = -1;
            for (size_t i = 0; i < folderBookmarks.size(); i++) {
                if (folderBookmarks[i].id == bookmarkId) {
                    sourcePosition = (int)i;
                    break;
                }
            }
            if (sourcePosition >= 0 && sourcePosition < finalPosition) {
                finalPosition--;
            }
        }
        bookmarks->MoveBookmark(bookmarkId, [targetFolder UTF8String], finalPosition);
    } else {
        // Moving from folder to root
        bookmarks->MoveBookmark(bookmarkId, "", finalPosition);
        // Adjust positions at root level
        bookmarks->MoveBookmarkAtRoot(bookmarkId, finalPosition);
    }

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
