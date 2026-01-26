#pragma once

#import <Cocoa/Cocoa.h>

// ============================================================================
// DS TEXT FIELD
// Styled text field with focus ring and container.
// ============================================================================

@interface DSTextField : NSView <NSTextFieldDelegate>

@property (nonatomic, readonly) NSTextField* textField;
@property (nonatomic, copy) NSString* text;
@property (nonatomic, copy) NSString* placeholder;
@property (nonatomic, assign, readonly) BOOL isFocused;

// Callbacks
@property (nonatomic, copy) void (^onSubmit)(NSString* text);
@property (nonatomic, copy) void (^onTextChange)(NSString* text);

- (void)focus;
- (void)blur;
- (void)selectAll;

@end

// ============================================================================
// DS SEARCH FIELD
// Text field with search icon and optional clear button.
// ============================================================================

@interface DSSearchField : DSTextField

@property (nonatomic, assign) BOOL showsClearButton;

@end
