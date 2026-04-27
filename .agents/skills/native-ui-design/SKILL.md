---
name: native-ui-design
description: Create distinctive, production-grade native desktop interfaces with high design quality. Use this skill when the user asks to build UI components, views, windows, or controls for macOS/Windows applications using C++, Objective-C++, or Cocoa/AppKit. Generates polished native code that avoids generic system defaults.
license: Apache 2.0 (derived from Anthropic frontend-design skill)
---

This skill guides creation of distinctive, production-grade native desktop interfaces that avoid generic "system default" aesthetics. Implement real working code with exceptional attention to visual details and thoughtful design choices.

The user provides native UI requirements: a component, view, window, panel, or control to build. They may include context about the purpose, audience, or technical constraints.

## Design Thinking

Before coding, understand the context and commit to a clear aesthetic direction:
- **Purpose**: What problem does this interface solve? Who uses it?
- **Tone**: Pick a direction: dark mode elegance, vibrant and playful, minimal and refined, brutalist/raw, soft and approachable, professional/corporate, retro/nostalgic, futuristic/tech, editorial/magazine, etc.
- **Platform**: macOS (Cocoa/AppKit), Windows (Win32/WinUI), or cross-platform considerations.
- **Differentiation**: What makes this MEMORABLE? What's the one visual element someone will remember?

**CRITICAL**: Choose a clear conceptual direction and execute it with precision. Bold designs and refined minimalism both work - the key is intentionality, not intensity.

Then implement working code (Objective-C++, C++, Cocoa/AppKit) that is:
- Production-grade and functional
- Visually striking and cohesive
- Thoughtfully refined in every detail
- Performant and responsive

## Native UI Aesthetics Guidelines

Focus on:

### Colors & Theming
- **Custom color palettes**: Don't rely solely on system colors. Define cohesive color schemes using `NSColor`/`CGColor` with intentional choices.
- **Dark mode excellence**: Design for dark backgrounds first. Rich, deep backgrounds (not pure black) with carefully chosen accent colors.
- **Color variables**: Use static functions or constants for color consistency across the codebase.
- **Contrast and hierarchy**: Use color intensity to guide attention. Muted backgrounds, vibrant accents.

```objc
// Define intentional colors, not generic system defaults
static NSColor* BackgroundColor() {
    return [NSColor colorWithRed:0.11 green:0.11 blue:0.12 alpha:1.0];
}
static NSColor* AccentColor() {
    return [NSColor colorWithRed:0.0 green:0.48 blue:1.0 alpha:1.0];
}
static NSColor* TextColor() {
    return [NSColor colorWithRed:0.9 green:0.9 blue:0.9 alpha:1.0];
}
```

### Typography
- **System fonts with weight**: Use `NSFont systemFontOfSize:weight:` with intentional weight choices (Light, Regular, Medium, Semibold, Bold).
- **Font hierarchy**: Clear distinction between titles, body text, and secondary text through size AND weight.
- **Custom fonts**: Load custom fonts when the design calls for distinctive typography.
- **Avoid defaults**: Don't accept system font defaults without consideration.

```objc
// Intentional typography hierarchy
NSFont* titleFont = [NSFont systemFontOfSize:18 weight:NSFontWeightSemibold];
NSFont* bodyFont = [NSFont systemFontOfSize:13 weight:NSFontWeightRegular];
NSFont* captionFont = [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
```

### Layout & Spacing
- **Consistent spacing system**: Use a base unit (4pt, 8pt) and multiples for all spacing.
- **Generous padding**: Don't crowd elements. Breathing room elevates design.
- **Alignment**: Consistent left/right margins throughout the interface.
- **Visual grouping**: Use spacing and subtle separators to create logical groups.

### Visual Effects & Depth
- **Layer-backed views**: Enable `wantsLayer = YES` for visual effects.
- **Corner radius**: Consistent, intentional corner radii (6pt, 8pt, 12pt) create modern feel.
- **Shadows**: Subtle shadows for elevation. Multiple shadow layers for depth.
- **Blur effects**: `NSVisualEffectView` for vibrancy and depth when appropriate.
- **Borders**: Subtle 1pt borders in darker/lighter shades for definition.

```objc
view.wantsLayer = YES;
view.layer.backgroundColor = BackgroundColor().CGColor;
view.layer.cornerRadius = 8.0;
view.layer.borderWidth = 1.0;
view.layer.borderColor = [NSColor colorWithWhite:1.0 alpha:0.1].CGColor;
```

### Animation & Motion
- **NSAnimationContext**: Use for coordinated animations with proper duration (0.2-0.3s for UI).
- **Implicit animations**: `allowsImplicitAnimation = YES` for smooth property changes.
- **Hover states**: Track mouse with `NSTrackingArea` for interactive feedback.
- **Transitions**: Fade, slide, or scale transitions for state changes.

```objc
[NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
    context.duration = 0.2;
    context.allowsImplicitAnimation = YES;
    view.animator.frame = newFrame;
    view.animator.alphaValue = 1.0;
} completionHandler:nil];
```

### Interactive Elements
- **Hover effects**: Background color changes, subtle scaling on hover.
- **Click feedback**: Visual response to mouse down events.
- **Focus states**: Clear indication of keyboard focus.
- **Tooltips**: Informative tooltips on controls via `toolTip` property.

### Custom Controls
- **Override `drawRect:`**: Custom drawing for unique appearances.
- **NSBezierPath**: For custom shapes, rounded rectangles, icons.
- **SF Symbols**: Use `[NSImage imageWithSystemSymbolName:]` for consistent iconography.
- **Attributed strings**: Rich text with multiple styles in single labels.

```objc
- (void)drawRect:(NSRect)dirtyRect {
    // Custom background
    [BackgroundColor() setFill];
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                         xRadius:8
                                                         yRadius:8];
    [path fill];

    // Custom content drawing...
}
```

## Anti-Patterns to Avoid

NEVER use:
- Generic system appearances without customization
- Default window chrome without `titlebarAppearsTransparent`
- Unstyled NSButton/NSTextField without visual refinement
- Hard-coded magic numbers without constants
- Pure black (#000) or pure white (#FFF) - use slightly off values
- Inconsistent spacing and alignment
- Missing hover/interaction states on clickable elements
- Heavy borders when subtle ones suffice

## Platform-Specific Notes

### macOS (Cocoa/AppKit)
- Use `NSWindowStyleMaskFullSizeContentView` for content under titlebar
- `titlebarAppearsTransparent = YES` for seamless designs
- `NSVisualEffectView` with `.behindWindow` for system vibrancy
- `NSTrackingArea` for hover detection
- `NSAnimationContext` for animations

### Cross-Platform (C++)
- Abstract color/font definitions into platform-agnostic interfaces
- Use consistent spacing constants across platforms
- Design system that maps to both Cocoa and Win32/WinUI

## Implementation Checklist

Before considering UI work complete:
- [ ] Colors defined as reusable constants/functions
- [ ] Typography hierarchy established
- [ ] Consistent spacing throughout
- [ ] Hover states on interactive elements
- [ ] Smooth animations for state changes
- [ ] Dark mode optimized (or explicit light mode choice)
- [ ] No default system appearance "leaking through"
- [ ] Visual feedback on all clickable elements

Remember: Native UI can be just as beautiful as web interfaces. Don't settle for system defaults - create something memorable.
