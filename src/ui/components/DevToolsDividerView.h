#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Draggable divider between browser content and DevTools panel
@interface DevToolsDividerView : NSView

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, readonly) BOOL isDragging;

@end
