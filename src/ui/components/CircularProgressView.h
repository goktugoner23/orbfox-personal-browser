#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// CIRCULAR PROGRESS VIEW
// Rounded rectangle progress ring for downloads icon
// (matches icon button shape, animates clockwise from top-center)
// ============================================================================

@interface CircularProgressView : NSView

@property (nonatomic, assign) CGFloat progress;  // 0.0 to 1.0
@property (nonatomic, strong) NSColor* trackColor;
@property (nonatomic, strong) NSColor* progressColor;
@property (nonatomic, assign) CGFloat lineWidth;
@property (nonatomic, assign) CGFloat cornerRadius;

@end
