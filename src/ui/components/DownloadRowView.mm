#import "DownloadRowView.h"
#import "SidebarView.h"
#import "DesignSystem.h"
#include "download_manager.h"

@implementation DownloadRowView {
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _isSelected = NO;
        _isHovered = NO;
    }
    return self;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                 options:(NSTrackingMouseEnteredAndExited |
                                                         NSTrackingActiveInKeyWindow)
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    _isHovered = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    // Select this row
    [_sidebarView selectDownloadRow:self];
    [super mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event {
    if (event.clickCount == 2) {
        [self handleDoubleClick];
    }
    [super mouseUp:event];
}

- (void)handleDoubleClick {
    if (_isFileMissing) {
        // File was deleted - restart download with previous choice
        if (_downloadUrl) {
            [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
        }
    } else if (_isComplete) {
        // Open completed download
        if (_downloadPath) {
            [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:_downloadPath]];
        }
    } else if (_isStopped) {
        // Restart stopped download - open URL in browser
        if (_downloadUrl) {
            [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
        }
    }
    // For in-progress downloads, double-click does nothing
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Draw selection/hover background
    if (_isSelected) {
        [[[DSColors accent] colorWithAlphaComponent:0.2] set];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                            xRadius:self.layer.cornerRadius
                                                            yRadius:self.layer.cornerRadius];
        [path fill];
    } else if (_isHovered) {
        [[[DSColors surface] colorWithAlphaComponent:0.5] set];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                            xRadius:self.layer.cornerRadius
                                                            yRadius:self.layer.cornerRadius];
        [path fill];
    }
}

- (NSMenu*)menuForEvent:(NSEvent*)event {
    (void)event;
    // Select the row when right-clicking
    [_sidebarView selectDownloadRow:self];

    NSMenu* menu = [[NSMenu alloc] init];

    if (_isInProgress) {
        // In-progress download menu
        NSMenuItem* stopItem = [[NSMenuItem alloc] initWithTitle:@"Stop"
                                                          action:@selector(stopDownload:)
                                                   keyEquivalent:@""];
        stopItem.target = self;
        [menu addItem:stopItem];
    } else if (_isComplete) {
        // Completed download menu
        NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                          action:@selector(openDownload:)
                                                   keyEquivalent:@""];
        openItem.target = self;
        [menu addItem:openItem];

        NSMenuItem* showItem = [[NSMenuItem alloc] initWithTitle:@"Show in Finder"
                                                          action:@selector(showInFinder:)
                                                   keyEquivalent:@""];
        showItem.target = self;
        [menu addItem:showItem];
    } else if (_isStopped) {
        // Stopped download menu
        NSMenuItem* restartItem = [[NSMenuItem alloc] initWithTitle:@"Restart Download"
                                                             action:@selector(restartDownloadAction:)
                                                      keyEquivalent:@""];
        restartItem.target = self;
        [menu addItem:restartItem];
    }

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* copyUrlItem = [[NSMenuItem alloc] initWithTitle:@"Copy Download Address"
                                                         action:@selector(copyDownloadUrl:)
                                                  keyEquivalent:@""];
    copyUrlItem.target = self;
    [menu addItem:copyUrlItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* removeItem = [[NSMenuItem alloc] initWithTitle:@"Remove from List"
                                                        action:@selector(removeDownload:)
                                                 keyEquivalent:@""];
    removeItem.target = self;
    [menu addItem:removeItem];

    return menu;
}

- (void)openDownload:(id)sender {
    (void)sender;
    if (_downloadPath) {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:_downloadPath]];
    }
}

- (void)showInFinder:(id)sender {
    (void)sender;
    if (_downloadPath) {
        [[NSWorkspace sharedWorkspace] selectFile:_downloadPath inFileViewerRootedAtPath:@""];
    }
}

- (void)stopDownload:(id)sender {
    (void)sender;
    // Cancel the download through the manager (which calls CEF cancel callback)
    DownloadManager::GetInstance().CancelDownload(_downloadId);
    [_sidebarView reloadDownloads];
}

- (void)restartDownloadAction:(id)sender {
    (void)sender;
    if (_downloadUrl) {
        [_sidebarView restartDownload:_downloadUrl removingDownloadId:_downloadId];
    }
}

- (void)copyDownloadUrl:(id)sender {
    (void)sender;
    if (_downloadUrl) {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:_downloadUrl forType:NSPasteboardTypeString];
    }
}

- (void)removeDownload:(id)sender {
    (void)sender;
    // Just remove from list, don't cancel if in progress
    DownloadManager::GetInstance().RemoveDownload(_downloadId);
    [_sidebarView reloadDownloads];
}

@end
