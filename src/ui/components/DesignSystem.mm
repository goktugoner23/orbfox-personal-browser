#import "DesignSystem.h"

// ============================================================================
// COLORS
// ============================================================================

@implementation DSColors

// Backgrounds
+ (NSColor*)background {
    return [NSColor colorWithRed:0.11 green:0.11 blue:0.12 alpha:1.0];
}

+ (NSColor*)backgroundSecondary {
    return [NSColor colorWithRed:0.08 green:0.08 blue:0.09 alpha:1.0];
}

+ (NSColor*)backgroundTertiary {
    return [NSColor colorWithRed:0.06 green:0.06 blue:0.07 alpha:1.0];
}

+ (NSColor*)surface {
    return [NSColor colorWithRed:0.18 green:0.18 blue:0.20 alpha:1.0];
}

+ (NSColor*)surfaceHover {
    return [NSColor colorWithRed:0.22 green:0.22 blue:0.24 alpha:1.0];
}

+ (NSColor*)surfaceActive {
    return [NSColor colorWithRed:0.26 green:0.26 blue:0.28 alpha:1.0];
}

// Text
+ (NSColor*)textPrimary {
    return [NSColor colorWithRed:0.9 green:0.9 blue:0.9 alpha:1.0];
}

+ (NSColor*)textSecondary {
    return [NSColor colorWithRed:0.6 green:0.6 blue:0.6 alpha:1.0];
}

+ (NSColor*)textDisabled {
    return [NSColor colorWithRed:0.35 green:0.35 blue:0.38 alpha:1.0];
}

// Accent
+ (NSColor*)accent {
    return [NSColor colorWithRed:0.0 green:0.48 blue:1.0 alpha:1.0];
}

+ (NSColor*)accentHover {
    return [NSColor colorWithRed:0.1 green:0.55 blue:1.0 alpha:1.0];
}

+ (NSColor*)accentSubtle {
    return [NSColor colorWithRed:0.0 green:0.48 blue:1.0 alpha:0.25];
}

// Semantic
+ (NSColor*)success {
    return [NSColor colorWithRed:0.3 green:0.7 blue:0.4 alpha:1.0];
}

+ (NSColor*)warning {
    return [NSColor colorWithRed:0.9 green:0.6 blue:0.2 alpha:1.0];
}

+ (NSColor*)error {
    return [NSColor colorWithRed:0.9 green:0.3 blue:0.3 alpha:1.0];
}

// Borders
+ (NSColor*)border {
    return [NSColor colorWithRed:0.25 green:0.25 blue:0.27 alpha:1.0];
}

+ (NSColor*)borderSubtle {
    return [NSColor colorWithWhite:1.0 alpha:0.08];
}

@end

// ============================================================================
// TYPOGRAPHY
// ============================================================================

@implementation DSTypography

+ (NSFont*)fontWithStyle:(DSFontStyle)style {
    switch (style) {
        case DSFontStyleTitle:
            return [NSFont systemFontOfSize:18 weight:NSFontWeightSemibold];
        case DSFontStyleHeadline:
            return [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];
        case DSFontStyleBody:
            return [NSFont systemFontOfSize:13 weight:NSFontWeightRegular];
        case DSFontStyleBodyMedium:
            return [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
        case DSFontStyleCaption:
            return [NSFont systemFontOfSize:11 weight:NSFontWeightRegular];
        case DSFontStyleCaptionMedium:
            return [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
    }
}

+ (NSDictionary*)attributesWithStyle:(DSFontStyle)style color:(NSColor*)color {
    return @{
        NSFontAttributeName: [self fontWithStyle:style],
        NSForegroundColorAttributeName: color
    };
}

@end

// ============================================================================
// SPACING
// ============================================================================

@implementation DSSpacing

+ (CGFloat)xxs { return 2.0; }
+ (CGFloat)xs  { return 4.0; }
+ (CGFloat)sm  { return 8.0; }
+ (CGFloat)md  { return 12.0; }
+ (CGFloat)lg  { return 16.0; }
+ (CGFloat)xl  { return 24.0; }
+ (CGFloat)xxl { return 32.0; }

@end

// ============================================================================
// LAYOUT
// ============================================================================

@implementation DSLayout

+ (CGFloat)cornerRadiusSmall  { return 4.0; }
+ (CGFloat)cornerRadiusMedium { return 6.0; }
+ (CGFloat)cornerRadiusLarge  { return 8.0; }
+ (CGFloat)cornerRadiusXLarge { return 12.0; }

+ (CGFloat)iconSizeSmall  { return 16.0; }
+ (CGFloat)iconSizeMedium { return 20.0; }
+ (CGFloat)iconSizeLarge  { return 28.0; }

+ (CGFloat)buttonHeight  { return 28.0; }
+ (CGFloat)rowHeight     { return 36.0; }
+ (CGFloat)toolbarHeight { return 44.0; }

@end

// ============================================================================
// ANIMATION
// ============================================================================

@implementation DSAnimation

+ (NSTimeInterval)durationFast   { return 0.15; }
+ (NSTimeInterval)durationNormal { return 0.25; }
+ (NSTimeInterval)durationSlow   { return 0.4; }

+ (void)animateWithDuration:(NSTimeInterval)duration
                 animations:(void (^)(void))animations {
    [self animateWithDuration:duration animations:animations completion:nil];
}

+ (void)animateWithDuration:(NSTimeInterval)duration
                 animations:(void (^)(void))animations
                 completion:(void (^)(void))completion {
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = duration;
        context.allowsImplicitAnimation = YES;
        if (animations) animations();
    } completionHandler:completion];
}

@end
