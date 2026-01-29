#pragma once

#import <Cocoa/Cocoa.h>

@class SidebarView;

// Individual tab row in the sidebar
@interface TabRowView : NSView

@property (nonatomic, assign) int tabId;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, assign) BOOL isLoading;
@property (nonatomic, assign) BOOL isPinned;
@property (nonatomic, assign) BOOL isMuted;
@property (nonatomic, strong) NSImage* favicon;
@property (nonatomic, weak) SidebarView* sidebarView;

@end
