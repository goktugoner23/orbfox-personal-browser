#pragma once

#import <Cocoa/Cocoa.h>

// Gesture types recognized by the container
typedef NS_ENUM(NSInteger, GestureType) {
    GestureTypeNone,
    GestureTypeLeft,      // Drag left → Go back
    GestureTypeRight,     // Drag right → Go forward
    GestureTypeLShape,    // Down then right → Close tab
};

@protocol GestureContainerDelegate <NSObject>
- (void)gestureRecognized:(GestureType)gesture;
@end

// A container view that recognizes right-click drag gestures (Vivaldi-style)
@interface GestureContainerView : NSView

@property (nonatomic, weak) id<GestureContainerDelegate> gestureDelegate;
@property (nonatomic, assign) BOOL gesturesEnabled;

// Minimum drag distance to recognize a gesture (default: 50pt)
@property (nonatomic, assign) CGFloat minimumGestureDistance;

@end
