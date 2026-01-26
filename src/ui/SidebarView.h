#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Sidebar panel type
typedef NS_ENUM(NSInteger, SidebarPanel) {
    SidebarPanelTabs,
    SidebarPanelFavorites,
    SidebarPanelHistory,
    SidebarPanelDownloads
};

// Sidebar view containing icon strip, workspace selector, and tab list
@interface SidebarView : NSView

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, assign) SidebarPanel activePanel;

- (void)reloadTabs;
- (void)selectTab:(int)tabId;
- (void)updateTab:(int)tabId title:(NSString*)title isLoading:(BOOL)isLoading;
- (void)updateTab:(int)tabId faviconData:(NSData*)faviconData;

@end

// Individual tab row in the sidebar
@interface TabRowView : NSView

@property (nonatomic, assign) int tabId;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, assign) BOOL isLoading;
@property (nonatomic, strong) NSImage* favicon;
@property (nonatomic, weak) SidebarView* sidebarView;

@end
