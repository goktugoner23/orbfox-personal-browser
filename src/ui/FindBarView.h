#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

@interface FindBarView : NSView <NSTextFieldDelegate>

@property (nonatomic, weak) MainWindowController* windowController;
@property (nonatomic, copy) void (^onSearchChanged)(NSString* text);
@property (nonatomic, copy) void (^onNext)(void);
@property (nonatomic, copy) void (^onPrev)(void);
@property (nonatomic, copy) void (^onClose)(void);

- (void)focusSearchField;
- (void)updateMatchCount:(int)count activeMatch:(int)activeMatch;
- (NSString*)searchText;

@end
