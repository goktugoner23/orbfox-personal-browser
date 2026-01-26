#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// DS ROW
// Reusable row component with hover effect.
// Used for list items like tabs, history entries, menu items.
// ============================================================================

@interface DSRow : NSView <NSDraggingSource>

@property (nonatomic, assign) BOOL isSelected;
@property (nonatomic, assign, readonly) BOOL isHovered;
@property (nonatomic, assign) BOOL showsCloseButton;
@property (nonatomic, assign) BOOL isDraggable;

// Content
@property (nonatomic, strong) NSImage* icon;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, copy) NSString* subtitle;

// Drag data (for draggable rows)
@property (nonatomic, copy) NSString* dragType;
@property (nonatomic, copy) NSDictionary* dragData;

// Callbacks
@property (nonatomic, copy) void (^onClick)(void);
@property (nonatomic, copy) void (^onDoubleClick)(void);
@property (nonatomic, copy) void (^onClose)(void);
@property (nonatomic, copy) void (^onRightClick)(NSEvent* event);
@property (nonatomic, copy) void (^onDragStarted)(void);

// For subclasses
- (void)setupSubviews;
- (void)updateAppearance;

@end

// ============================================================================
// DS TAB ROW
// Specialized row for browser tabs with favicon and loading indicator.
// ============================================================================

@interface DSTabRow : DSRow

@property (nonatomic, assign) int tabId;
@property (nonatomic, assign) BOOL isLoading;
@property (nonatomic, strong) NSImage* favicon;

@end

// ============================================================================
// DS HISTORY ROW
// Specialized row for history entries with URL and time.
// ============================================================================

@interface DSHistoryRow : DSRow

@property (nonatomic, copy) NSString* url;
@property (nonatomic, copy) NSString* time;

@end
