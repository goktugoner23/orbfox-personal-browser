#import "AccountPopover.h"
#import "MainWindowController.h"
#import "ToolbarView.h"
#import "components/Components.h"
#include "sync/google_auth.h"
#include "sync/sync_service.h"

@implementation AccountPopoverController {
    NSPopover* _popover;

    // Signed out UI
    NSView* _signedOutView;
    NSButton* _signInButton;

    // Signed in UI
    NSView* _signedInView;
    NSImageView* _avatarView;
    NSTextField* _nameLabel;
    NSTextField* _emailLabel;
    NSTextField* _syncStatusLabel;
    NSButton* _syncNowButton;
    NSButton* _signOutButton;

    // Sync toggles (future)
    // NSButton* _syncBookmarksToggle;
    // NSButton* _syncHistoryToggle;
    // NSButton* _syncSettingsToggle;
}

- (void)loadView {
    CGFloat width = 280;
    CGFloat height = 200;

    NSView* contentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    contentView.wantsLayer = YES;

    // Create both views
    [self createSignedOutView:contentView width:width height:height];
    [self createSignedInView:contentView width:width height:height];

    self.view = contentView;

    // Show appropriate view based on auth state
    [self updateAuthState];

    // Observe sync completion notifications
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(handleSyncCompleted:)
                                                 name:@"OrbFoxSyncCompleted"
                                               object:nil];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)createSignedOutView:(NSView*)parent width:(CGFloat)width height:(CGFloat)height {
    _signedOutView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];

    CGFloat padding = [DSSpacing md];
    CGFloat y = height - padding;

    // Icon
    CGFloat iconSize = 48;
    y -= iconSize;
    NSImageView* icon = [[NSImageView alloc] initWithFrame:NSMakeRect((width - iconSize) / 2, y, iconSize, iconSize)];
    NSImage* personIcon = [NSImage imageWithSystemSymbolName:@"person.circle"
                                   accessibilityDescription:@"Account"];
    NSImageSymbolConfiguration* config = [NSImageSymbolConfiguration configurationWithPointSize:iconSize
                                                                                        weight:NSFontWeightLight];
    icon.image = [personIcon imageWithSymbolConfiguration:config];
    icon.contentTintColor = [DSColors textSecondary];
    [_signedOutView addSubview:icon];

    // Title
    y -= 24 + [DSSpacing sm];
    NSTextField* title = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 24)];
    title.stringValue = @"Sign in to OrbFox";
    title.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    title.textColor = [DSColors textPrimary];
    title.alignment = NSTextAlignmentCenter;
    title.bezeled = NO;
    title.drawsBackground = NO;
    title.editable = NO;
    [_signedOutView addSubview:title];

    // Description
    y -= 40 + [DSSpacing xs];
    NSTextField* desc = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 40)];
    desc.stringValue = @"Sync your bookmarks, history, and settings across all your devices.";
    desc.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    desc.textColor = [DSColors textSecondary];
    desc.alignment = NSTextAlignmentCenter;
    desc.bezeled = NO;
    desc.drawsBackground = NO;
    desc.editable = NO;
    desc.usesSingleLineMode = NO;
    desc.lineBreakMode = NSLineBreakByWordWrapping;
    [desc.cell setWraps:YES];
    [_signedOutView addSubview:desc];

    // Sign in button
    CGFloat buttonWidth = 200;
    CGFloat buttonHeight = 36;
    y -= buttonHeight + [DSSpacing md];
    _signInButton = [[NSButton alloc] initWithFrame:NSMakeRect((width - buttonWidth) / 2, y, buttonWidth, buttonHeight)];
    _signInButton.title = @"Sign in with Google";
    _signInButton.bezelStyle = NSBezelStyleRounded;
    _signInButton.target = self;
    _signInButton.action = @selector(signInClicked:);

    // Style as primary button
    _signInButton.wantsLayer = YES;
    _signInButton.layer.backgroundColor = [DSColors accent].CGColor;
    _signInButton.layer.cornerRadius = [DSLayout cornerRadiusSmall];
    _signInButton.contentTintColor = [NSColor whiteColor];

    [_signedOutView addSubview:_signInButton];

    [parent addSubview:_signedOutView];
}

- (void)createSignedInView:(NSView*)parent width:(CGFloat)width height:(CGFloat)height {
    _signedInView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];

    CGFloat padding = [DSSpacing md];
    CGFloat y = height - padding;

    // Avatar
    CGFloat avatarSize = 56;
    y -= avatarSize;
    _avatarView = [[NSImageView alloc] initWithFrame:NSMakeRect((width - avatarSize) / 2, y, avatarSize, avatarSize)];
    _avatarView.wantsLayer = YES;
    _avatarView.layer.cornerRadius = avatarSize / 2;
    _avatarView.layer.masksToBounds = YES;
    _avatarView.layer.backgroundColor = [DSColors surface].CGColor;
    _avatarView.imageScaling = NSImageScaleProportionallyUpOrDown;

    // Default avatar
    NSImage* defaultAvatar = [NSImage imageWithSystemSymbolName:@"person.circle.fill"
                                      accessibilityDescription:@"Account"];
    NSImageSymbolConfiguration* config = [NSImageSymbolConfiguration configurationWithPointSize:avatarSize - 8
                                                                                        weight:NSFontWeightRegular];
    _avatarView.image = [defaultAvatar imageWithSymbolConfiguration:config];
    _avatarView.contentTintColor = [DSColors textSecondary];
    [_signedInView addSubview:_avatarView];

    // Name
    y -= 20 + [DSSpacing sm];
    _nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 20)];
    _nameLabel.stringValue = @"User Name";
    _nameLabel.font = [DSTypography fontWithStyle:DSFontStyleHeadline];
    _nameLabel.textColor = [DSColors textPrimary];
    _nameLabel.alignment = NSTextAlignmentCenter;
    _nameLabel.bezeled = NO;
    _nameLabel.drawsBackground = NO;
    _nameLabel.editable = NO;
    [_signedInView addSubview:_nameLabel];

    // Email
    y -= 16 + 2;
    _emailLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 16)];
    _emailLabel.stringValue = @"user@example.com";
    _emailLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _emailLabel.textColor = [DSColors textSecondary];
    _emailLabel.alignment = NSTextAlignmentCenter;
    _emailLabel.bezeled = NO;
    _emailLabel.drawsBackground = NO;
    _emailLabel.editable = NO;
    [_signedInView addSubview:_emailLabel];

    // Sync status
    y -= 16 + [DSSpacing md];
    _syncStatusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(padding, y, width - padding * 2, 16)];
    _syncStatusLabel.stringValue = @"Synced just now";
    _syncStatusLabel.font = [DSTypography fontWithStyle:DSFontStyleCaption];
    _syncStatusLabel.textColor = [DSColors textDisabled];
    _syncStatusLabel.alignment = NSTextAlignmentCenter;
    _syncStatusLabel.bezeled = NO;
    _syncStatusLabel.drawsBackground = NO;
    _syncStatusLabel.editable = NO;
    [_signedInView addSubview:_syncStatusLabel];

    // Buttons row
    y -= 28 + [DSSpacing sm];
    CGFloat buttonWidth = 100;
    CGFloat buttonHeight = 28;
    CGFloat buttonSpacing = [DSSpacing sm];
    CGFloat totalButtonWidth = buttonWidth * 2 + buttonSpacing;
    CGFloat buttonX = (width - totalButtonWidth) / 2;

    // Sync now button
    _syncNowButton = [[NSButton alloc] initWithFrame:NSMakeRect(buttonX, y, buttonWidth, buttonHeight)];
    _syncNowButton.title = @"Sync Now";
    _syncNowButton.bezelStyle = NSBezelStyleRounded;
    _syncNowButton.target = self;
    _syncNowButton.action = @selector(syncNowClicked:);
    [_signedInView addSubview:_syncNowButton];

    // Sign out button
    _signOutButton = [[NSButton alloc] initWithFrame:NSMakeRect(buttonX + buttonWidth + buttonSpacing, y, buttonWidth, buttonHeight)];
    _signOutButton.title = @"Sign Out";
    _signOutButton.bezelStyle = NSBezelStyleRounded;
    _signOutButton.target = self;
    _signOutButton.action = @selector(signOutClicked:);
    [_signedInView addSubview:_signOutButton];

    [parent addSubview:_signedInView];
}

- (void)updateAuthState {
    GoogleAuth& auth = GoogleAuth::GetInstance();
    bool signedIn = auth.IsSignedIn();

    _signedOutView.hidden = signedIn;
    _signedInView.hidden = !signedIn;

    if (signedIn) {
        UserProfile profile = auth.GetUserProfile();

        // Update name
        NSString* name = [NSString stringWithUTF8String:profile.display_name.c_str()];
        _nameLabel.stringValue = (name.length > 0) ? name : @"OrbFox User";

        // Update email
        NSString* email = [NSString stringWithUTF8String:profile.email.c_str()];
        _emailLabel.stringValue = email ?: @"";

        // Load avatar if URL available
        if (!profile.avatar_url.empty()) {
            [self loadAvatarFromURL:[NSString stringWithUTF8String:profile.avatar_url.c_str()]];
        }

        // Update sync status
        // TODO: Get actual sync status from SyncService
        _syncStatusLabel.stringValue = @"Not synced yet";
    }
}

- (void)loadAvatarFromURL:(NSString*)urlString {
    if (!urlString || urlString.length == 0) return;

    NSURL* url = [NSURL URLWithString:urlString];
    if (!url) return;

    NSURLSessionDataTask* task = [[NSURLSession sharedSession]
        dataTaskWithURL:url
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            (void)response;
            if (error || !data) return;

            NSImage* image = [[NSImage alloc] initWithData:data];
            if (!image) return;

            dispatch_async(dispatch_get_main_queue(), ^{
                self->_avatarView.image = image;
                self->_avatarView.contentTintColor = nil;
            });
        }];
    [task resume];
}

- (void)showRelativeToView:(NSView*)view {
    if (!self.view) {
        [self loadView];
    }

    [self updateAuthState];

    if (!_popover) {
        _popover = [[NSPopover alloc] init];
        _popover.contentViewController = self;
        _popover.behavior = NSPopoverBehaviorTransient;
    }

    [_popover showRelativeToRect:view.bounds
                          ofView:view
                   preferredEdge:NSRectEdgeMinY];
}

- (void)close {
    [_popover close];
}

#pragma mark - Actions

- (void)signInClicked:(id)sender {
    (void)sender;

    // Start sign-in flow
    GoogleAuth& auth = GoogleAuth::GetInstance();

    // Set up URL opener to open auth URL in OrbFox (not default browser)
    // IMPORTANT: Don't capture Objective-C objects in C++ lambdas - ARC doesn't work with them
    // Instead, we look up the window controller fresh from the main thread
    auth.SetUrlOpener([](const std::string& url) {
        // Make a copy of the URL immediately (url reference may not survive async dispatch)
        std::string urlCopy = url;

        dispatch_async(dispatch_get_main_queue(), ^{
            NSString* nsUrl = [[NSString alloc] initWithUTF8String:urlCopy.c_str()];
            if (!nsUrl || nsUrl.length == 0) {
                NSLog(@"OAuth: Invalid URL string");
                return;
            }

            NSLog(@"OAuth: Opening URL in OrbFox: %@", nsUrl);

            // Get the main window controller fresh - don't rely on captured pointers
            NSWindow* mainWindow = [[NSApplication sharedApplication] mainWindow];
            if (mainWindow && [mainWindow.windowController isKindOfClass:[MainWindowController class]]) {
                MainWindowController* controller = (MainWindowController*)mainWindow.windowController;
                [controller openUrlInNewTab:nsUrl];
            } else {
                // Fallback: open in system browser if OrbFox window not available
                [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:nsUrl]];
            }
        });
    });

    // Disable button and show loading state
    _signInButton.enabled = NO;
    _signInButton.title = @"Signing in...";

    // IMPORTANT: Don't capture __weak Objective-C pointers in C++ lambdas!
    // C++ doesn't understand Objective-C weak references - they become dangling pointers.
    // Instead, copy all needed data and look up objects fresh from main thread.
    auth.StartSignIn([](const AuthResult& result) {
        // Copy all result data immediately - result reference won't survive async dispatch
        bool success = result.success;
        std::string errorMsg = result.error_message;
        std::string avatarUrl = result.profile.avatar_url;

        dispatch_async(dispatch_get_main_queue(), ^{
            // Look up the main window controller fresh - safe to do on main thread
            NSWindow* mainWindow = [[NSApplication sharedApplication] mainWindow];
            MainWindowController* windowController = nil;
            if (mainWindow && [mainWindow.windowController isKindOfClass:[MainWindowController class]]) {
                windowController = (MainWindowController*)mainWindow.windowController;
            }

            if (success) {
                // Update toolbar if available
                if (windowController) {
                    [windowController.toolbarView setSignedIn:YES];
                    if (!avatarUrl.empty()) {
                        NSString* nsAvatarUrl = [NSString stringWithUTF8String:avatarUrl.c_str()];
                        [windowController.toolbarView setAvatarURL:nsAvatarUrl];
                    }

                    // Close the OAuth callback tab (localhost:XXXXX/oauth/callback)
                    TabManager* tabManager = windowController.tabManager;
                    if (tabManager) {
                        for (const auto& workspace : tabManager->GetWorkspaces()) {
                            for (const auto& tab : workspace->tabs) {
                                if (tab && tab->url.find("localhost") != std::string::npos &&
                                    tab->url.find("/oauth/callback") != std::string::npos) {
                                    tabManager->CloseTab(tab->id);
                                    break;  // Only close one
                                }
                            }
                        }
                    }
                }

                // Post notification for any interested parties (like the account popover)
                [[NSNotificationCenter defaultCenter] postNotificationName:@"OrbFoxSignInCompleted"
                                                                    object:nil
                                                                  userInfo:@{@"success": @YES}];
            } else {
                // Show error
                NSAlert* alert = [[NSAlert alloc] init];
                alert.messageText = @"Sign In Failed";
                alert.informativeText = [NSString stringWithUTF8String:errorMsg.c_str()];
                alert.alertStyle = NSAlertStyleWarning;
                [alert addButtonWithTitle:@"OK"];
                [alert runModal];

                // Post notification for failure
                [[NSNotificationCenter defaultCenter] postNotificationName:@"OrbFoxSignInCompleted"
                                                                    object:nil
                                                                  userInfo:@{@"success": @NO}];
            }
        });
    });

    // Listen for sign-in completion to update our UI
    [[NSNotificationCenter defaultCenter] addObserverForName:@"OrbFoxSignInCompleted"
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification* note) {
        // Re-enable button
        self->_signInButton.enabled = YES;
        self->_signInButton.title = @"Sign in with Google";

        BOOL success = [note.userInfo[@"success"] boolValue];
        if (success) {
            [self updateAuthState];
            if (self.onSignIn) {
                self.onSignIn();
            }
        }

        // Remove this observer after handling
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:@"OrbFoxSignInCompleted"
                                                      object:nil];
    }];
}

- (void)signOutClicked:(id)sender {
    (void)sender;

    // Confirm sign out
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Sign Out";
    alert.informativeText = @"Are you sure you want to sign out? Your local data will remain on this device.";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"Sign Out"];
    [alert addButtonWithTitle:@"Cancel"];

    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        GoogleAuth& auth = GoogleAuth::GetInstance();
        auth.SignOut();

        [self updateAuthState];

        // Notify toolbar to update avatar
        if (self.windowController) {
            [self.windowController.toolbarView setSignedIn:NO];
        }

        if (self.onSignOut) {
            self.onSignOut();
        }
    }
}

- (void)syncNowClicked:(id)sender {
    (void)sender;

    // Update UI to show syncing
    _syncNowButton.enabled = NO;
    _syncNowButton.title = @"Syncing...";
    _syncStatusLabel.stringValue = @"Syncing...";

    // Trigger actual sync via SyncService
    // Using notification pattern to avoid capturing ObjC objects in C++ lambda
    SyncService::GetInstance().SyncNow([](const SyncResult& result) {
        bool success = result.success;
        std::string errorMsg = result.error_message;
        int bookmarks = result.bookmarks_synced;
        int history = result.history_synced;
        bool settings = result.settings_synced;

        dispatch_async(dispatch_get_main_queue(), ^{
            NSMutableDictionary* userInfo = [NSMutableDictionary dictionary];
            userInfo[@"success"] = @(success);
            if (!errorMsg.empty()) {
                userInfo[@"error"] = [NSString stringWithUTF8String:errorMsg.c_str()];
            }
            userInfo[@"bookmarks"] = @(bookmarks);
            userInfo[@"history"] = @(history);
            userInfo[@"settings"] = @(settings);

            [[NSNotificationCenter defaultCenter] postNotificationName:@"OrbFoxSyncCompleted"
                                                                object:nil
                                                              userInfo:userInfo];
        });
    });
}

- (void)handleSyncCompleted:(NSNotification*)notification {
    NSDictionary* info = notification.userInfo;
    BOOL success = [info[@"success"] boolValue];

    _syncNowButton.enabled = YES;
    _syncNowButton.title = @"Sync Now";

    if (success) {
        int bookmarks = [info[@"bookmarks"] intValue];
        int history = [info[@"history"] intValue];
        BOOL settings = [info[@"settings"] boolValue];

        NSMutableArray* parts = [NSMutableArray array];
        if (bookmarks > 0) [parts addObject:@"bookmarks"];
        if (history > 0) [parts addObject:@"history"];
        if (settings) [parts addObject:@"settings"];

        if (parts.count > 0) {
            _syncStatusLabel.stringValue = [NSString stringWithFormat:@"Synced %@",
                                           [parts componentsJoinedByString:@", "]];
        } else {
            _syncStatusLabel.stringValue = @"Synced (no changes)";
        }
    } else {
        NSString* error = info[@"error"];
        _syncStatusLabel.stringValue = error ?: @"Sync failed";
    }

    if (self.onSyncNow) {
        self.onSyncNow();
    }
}

@end
