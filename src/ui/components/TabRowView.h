#pragma once

#import <Cocoa/Cocoa.h>

@class SidebarView;

// Pasteboard type for tab dragging
extern NSPasteboardType const TabRowPasteboardType;

// Individual tab row in the sidebar
@interface TabRowView : NSView <NSDraggingSource>

@property (nonatomic, assign) int tabId;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, assign) BOOL isLoading;
@property (nonatomic, assign) BOOL isPinned;
@property (nonatomic, assign) BOOL isMuted;
@property (nonatomic, assign) BOOL isHibernated;
@property (nonatomic, strong) NSImage* favicon;
@property (nonatomic, weak) SidebarView* sidebarView;

@end
