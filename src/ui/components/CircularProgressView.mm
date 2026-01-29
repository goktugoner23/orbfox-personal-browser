#import "CircularProgressView.h"
#import "DesignSystem.h"

@implementation CircularProgressView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _progress = 0.0;
        _trackColor = [[DSColors textSecondary] colorWithAlphaComponent:0.3];
        _progressColor = [DSColors accent];
        _lineWidth = 2.5;
        _cornerRadius = [DSLayout cornerRadiusMedium];
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
    return self;
}

- (BOOL)isOpaque {
    return NO;
}

- (void)setProgress:(CGFloat)progress {
    _progress = progress;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    NSRect insetRect = NSInsetRect(self.bounds, _lineWidth / 2.0, _lineWidth / 2.0);
    CGFloat cr = MIN(_cornerRadius, MIN(insetRect.size.width, insetRect.size.height) / 2.0);

    // Draw track (background rounded rect)
    NSBezierPath* trackPath = [NSBezierPath bezierPathWithRoundedRect:insetRect
                                                              xRadius:cr
                                                              yRadius:cr];
    [_trackColor setStroke];
    trackPath.lineWidth = _lineWidth;
    [trackPath stroke];

    // Draw progress starting from top-center, going clockwise
    if (_progress > 0.001) {
        CGFloat x = NSMinX(insetRect);
        CGFloat y = NSMinY(insetRect);
        CGFloat w = NSWidth(insetRect);
        CGFloat h = NSHeight(insetRect);

        // Calculate perimeter segments (clockwise from top-center)
        CGFloat arcLength = cr * M_PI / 2.0;  // Quarter circle arc length
        CGFloat topHalf = (w - 2 * cr) / 2.0;
        CGFloat rightSide = h - 2 * cr;
        CGFloat bottomSide = w - 2 * cr;
        CGFloat leftSide = h - 2 * cr;

        CGFloat totalPerimeter = 2 * topHalf + 4 * arcLength + rightSide + bottomSide + leftSide;
        CGFloat targetLength = _progress * totalPerimeter;

        NSBezierPath* progressPath = [NSBezierPath bezierPath];
        progressPath.lineWidth = _lineWidth;
        progressPath.lineCapStyle = NSLineCapStyleRound;

        CGFloat midTopX = x + w / 2.0;
        CGFloat topY = y + h;
        [progressPath moveToPoint:NSMakePoint(midTopX, topY)];

        CGFloat drawn = 0;

        // Segment 1: Top-right half line
        if (drawn < targetLength) {
            CGFloat segLen = topHalf;
            CGFloat endX = x + w - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(endX, topY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(midTopX + (topHalf * ratio), topY)];
                drawn = targetLength;
            }
        }

        // Segment 2: Top-right corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + w - cr;
            CGFloat arcCenterY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:90
                                                       endAngle:0
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = 90 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:90
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 3: Right side
        if (drawn < targetLength) {
            CGFloat segLen = rightSide;
            CGFloat endY = y + cr;
            CGFloat rightX = x + w;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(rightX, endY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(rightX, (y + h - cr) - rightSide * ratio)];
                drawn = targetLength;
            }
        }

        // Segment 4: Bottom-right corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + w - cr;
            CGFloat arcCenterY = y + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:0
                                                       endAngle:-90
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = -90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:0
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 5: Bottom side
        if (drawn < targetLength) {
            CGFloat segLen = bottomSide;
            CGFloat endX = x + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(endX, y)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint((x + w - cr) - bottomSide * ratio, y)];
                drawn = targetLength;
            }
        }

        // Segment 6: Bottom-left corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + cr;
            CGFloat arcCenterY = y + cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:-90
                                                       endAngle:-180
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = -90 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:-90
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 7: Left side
        if (drawn < targetLength) {
            CGFloat segLen = leftSide;
            CGFloat endY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(x, endY)];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint(x, (y + cr) + leftSide * ratio)];
                drawn = targetLength;
            }
        }

        // Segment 8: Top-left corner arc
        if (drawn < targetLength) {
            CGFloat segLen = arcLength;
            CGFloat arcCenterX = x + cr;
            CGFloat arcCenterY = y + h - cr;
            if (drawn + segLen <= targetLength) {
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:180
                                                       endAngle:90
                                                      clockwise:YES];
                drawn += segLen;
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                CGFloat endAngle = 180 - 90 * ratio;
                [progressPath appendBezierPathWithArcWithCenter:NSMakePoint(arcCenterX, arcCenterY)
                                                         radius:cr
                                                     startAngle:180
                                                       endAngle:endAngle
                                                      clockwise:YES];
                drawn = targetLength;
            }
        }

        // Segment 9: Top-left half line
        if (drawn < targetLength) {
            CGFloat segLen = topHalf;
            if (drawn + segLen <= targetLength) {
                [progressPath lineToPoint:NSMakePoint(midTopX, topY)];
            } else {
                CGFloat ratio = (targetLength - drawn) / segLen;
                [progressPath lineToPoint:NSMakePoint((x + cr) + topHalf * ratio, topY)];
            }
        }

        [_progressColor setStroke];
        [progressPath stroke];
    }
}

@end
