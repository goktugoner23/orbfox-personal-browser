#pragma once

#import <Cocoa/Cocoa.h>

@class MainWindowController;

// Single autocomplete suggestion
@interface AutocompleteSuggestion : NSObject
@property (nonatomic, copy) NSString* title;
@property (nonatomic, copy) NSString* url;
@property (nonatomic, strong) NSImage* favicon;
@property (nonatomic, assign) BOOL isBookmarked;
@end

// Autocomplete dropdown for URL bar
@interface AutocompleteDropdownView : NSView

@property (nonatomic, assign) MainWindowController* windowController;
@property (nonatomic, copy) void (^onSelect)(NSString* url);
@property (nonatomic, copy) void (^onDismiss)(void);
@property (nonatomic, readonly) NSArray<AutocompleteSuggestion*>* suggestions;

- (void)updateSuggestionsForQuery:(NSString*)query;
- (void)show;
- (void)hide;
- (BOOL)isVisible;

// Keyboard navigation
- (void)moveSelectionUp;
- (void)moveSelectionDown;
- (NSString*)selectedURL;

@end
