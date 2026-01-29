#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// CLICKABLE VIEW
// Simple view that sends action to target on mouse click
// ============================================================================

@interface ClickableView : NSView
@property (nonatomic, weak) id target;
@property (nonatomic) SEL action;
@end

// ============================================================================
// HORIZONTAL SCROLL VIEW
// Scroll view that translates vertical scroll wheel to horizontal scrolling
// ============================================================================

@interface HorizontalScrollView : NSScrollView
@end

// ============================================================================
// FLIPPED VIEW
// NSView subclass with flipped coordinate system (top-to-bottom layout)
// ============================================================================

@interface FlippedView : NSView
@end
