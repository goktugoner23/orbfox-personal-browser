#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// DS BUTTON
// Base button component with hover and click feedback.
// Similar to a React Button component with variants.
// ============================================================================

typedef NS_ENUM(NSInteger, DSButtonVariant) {
    DSButtonVariantGhost,      // No background, shows on hover
    DSButtonVariantSubtle,     // Subtle background
    DSButtonVariantFilled,     // Filled accent background
};

typedef NS_ENUM(NSInteger, DSButtonSize) {
    DSButtonSizeSmall,         // 24pt height
    DSButtonSizeMedium,        // 28pt height
    DSButtonSizeLarge,         // 34pt height
};

@interface DSButton : NSButton

@property (nonatomic, assign) DSButtonVariant variant;
@property (nonatomic, assign) DSButtonSize size;
@property (nonatomic, assign, readonly) BOOL isHovered;

// Factory methods
+ (instancetype)buttonWithTitle:(NSString*)title
                        variant:(DSButtonVariant)variant;

+ (instancetype)buttonWithIcon:(NSString*)symbolName
                       variant:(DSButtonVariant)variant;

+ (instancetype)buttonWithTitle:(NSString*)title
                           icon:(NSString*)symbolName
                        variant:(DSButtonVariant)variant;

@end

// ============================================================================
// DS ICON BUTTON
// Compact icon-only button, commonly used in toolbars and sidebars.
// ============================================================================

@interface DSIconButton : DSButton

@property (nonatomic, copy) NSString* symbolName;

+ (instancetype)buttonWithIcon:(NSString*)symbolName;
+ (instancetype)buttonWithIcon:(NSString*)symbolName tooltip:(NSString*)tooltip;

@end
