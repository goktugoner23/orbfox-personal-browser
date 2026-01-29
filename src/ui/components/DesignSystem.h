#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// DESIGN SYSTEM
// Centralized theme configuration for consistent UI across the application.
// Similar to a CSS-in-JS theme or design tokens.
// ============================================================================

#pragma mark - Colors

@interface DSColors : NSObject

// Backgrounds
+ (NSColor*)background;           // Main background
+ (NSColor*)backgroundSecondary;  // Secondary/sidebar background
+ (NSColor*)backgroundTertiary;   // Even darker background
+ (NSColor*)surface;              // Card/panel surface
+ (NSColor*)surfaceHover;         // Hovered surface
+ (NSColor*)surfaceActive;        // Active/selected surface

// Text
+ (NSColor*)textPrimary;          // Main text
+ (NSColor*)textSecondary;        // Muted text
+ (NSColor*)textDisabled;         // Disabled text

// Accent
+ (NSColor*)accent;               // Primary accent color
+ (NSColor*)accentHover;          // Hovered accent
+ (NSColor*)accentSubtle;         // Subtle accent (for backgrounds)

// Semantic
+ (NSColor*)success;              // Success/secure
+ (NSColor*)warning;              // Warning
+ (NSColor*)error;                // Error/danger

// Borders
+ (NSColor*)border;               // Default border
+ (NSColor*)borderSubtle;         // Subtle border

// Dividers
+ (NSColor*)divider;              // Divider line background
+ (NSColor*)dividerGrip;          // Divider grip indicator

// Special Panels
+ (NSColor*)devToolsBackground;   // DevTools panel background

@end

#pragma mark - Typography

typedef NS_ENUM(NSInteger, DSFontStyle) {
    DSFontStyleTitle,             // 18pt Semibold
    DSFontStyleHeadline,          // 15pt Semibold
    DSFontStyleBody,              // 13pt Regular
    DSFontStyleBodyMedium,        // 13pt Medium
    DSFontStyleCaption,           // 11pt Regular
    DSFontStyleCaptionMedium,     // 11pt Medium
};

@interface DSTypography : NSObject

+ (NSFont*)fontWithStyle:(DSFontStyle)style;
+ (NSDictionary*)attributesWithStyle:(DSFontStyle)style color:(NSColor*)color;

@end

#pragma mark - Spacing

@interface DSSpacing : NSObject

// Base unit: 4pt
+ (CGFloat)xxs;   // 2pt
+ (CGFloat)xs;    // 4pt
+ (CGFloat)sm;    // 8pt
+ (CGFloat)md;    // 12pt
+ (CGFloat)lg;    // 16pt
+ (CGFloat)xl;    // 24pt
+ (CGFloat)xxl;   // 32pt

@end

#pragma mark - Layout

@interface DSLayout : NSObject

// Corner radii
+ (CGFloat)cornerRadiusSmall;     // 4pt
+ (CGFloat)cornerRadiusMedium;    // 6pt
+ (CGFloat)cornerRadiusLarge;     // 8pt
+ (CGFloat)cornerRadiusXLarge;    // 12pt

// Common sizes
+ (CGFloat)iconSizeSmall;         // 16pt
+ (CGFloat)iconSizeMedium;        // 20pt
+ (CGFloat)iconSizeLarge;         // 28pt

+ (CGFloat)buttonHeight;          // 28pt
+ (CGFloat)rowHeight;             // 36pt
+ (CGFloat)toolbarHeight;         // 44pt

@end

#pragma mark - Animation

@interface DSAnimation : NSObject

+ (NSTimeInterval)durationFast;   // 0.15s
+ (NSTimeInterval)durationNormal; // 0.25s
+ (NSTimeInterval)durationSlow;   // 0.4s

// Helper to run animated block
+ (void)animateWithDuration:(NSTimeInterval)duration
                 animations:(void (^)(void))animations;

+ (void)animateWithDuration:(NSTimeInterval)duration
                 animations:(void (^)(void))animations
                 completion:(void (^)(void))completion;

@end

#pragma mark - Convenience Macros

// Quick access to design system values
#define DSColor(name) [DSColors name]
#define DSFont(style) [DSTypography fontWithStyle:style]
#define DSSpace(size) [DSSpacing size]
#define DSRadius(size) [DSLayout cornerRadius##size]
