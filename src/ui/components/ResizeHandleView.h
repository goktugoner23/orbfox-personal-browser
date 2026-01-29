#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Draggable divider for resizing sidebar
@interface ResizeHandleView : NSView

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, readonly) BOOL isDragging;

@end
