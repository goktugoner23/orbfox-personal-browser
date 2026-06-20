#import <Cocoa/Cocoa.h>

#include "browser_app.h"
#include "bookmark_storage.h"
#include "bookmark_importer.h"
#include "history_storage.h"

#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"

// Import MainWindowController for window lookup during shutdown
#import "MainWindowController.h"

// Application delegate
@interface AppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate> {
    NSMenu* _bookmarksMenu;
    NSMenu* _historyMenu;
}
- (void)setupMenuBar;
- (void)populateBookmarksMenu:(NSMenu*)menu;
- (void)populateHistoryMenu:(NSMenu*)menu;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    // CEF is initialized in main(), browser is created in OnContextInitialized
    [self setupMenuBar];
    // Clear any stale dock badge (e.g. left over from a web-notification count)
    [NSApp dockTile].badgeLabel = nil;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return NO;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)hasVisibleWindows {
    (void)sender;
    if (!hasVisibleWindows) {
        // Show the hidden main window when user clicks dock icon
        for (NSWindow* window in [NSApp windows]) {
            if ([window.windowController isKindOfClass:[MainWindowController class]]) {
                [window makeKeyAndOrderFront:nil];
                return NO;
            }
        }
    }
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    return NSTerminateNow;
}

- (void)setupMenuBar {
    NSMenu* mainMenu = [[NSMenu alloc] init];

    // Application menu
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"OrbFox"];
    [appMenu addItemWithTitle:@"About OrbFox" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide OrbFox" action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem* hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit OrbFox" action:@selector(terminate:) keyEquivalent:@"q"];
    appMenuItem.submenu = appMenu;
    [mainMenu addItem:appMenuItem];

    // File menu
    NSMenuItem* fileMenuItem = [[NSMenuItem alloc] init];
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"New Tab" action:@selector(newTab:) keyEquivalent:@"t"];
    [fileMenu addItemWithTitle:@"Close Tab" action:@selector(closeTab:) keyEquivalent:@"w"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"W"];
    fileMenuItem.submenu = fileMenu;
    [mainMenu addItem:fileMenuItem];

    // Edit menu
    NSMenuItem* editMenuItem = [[NSMenuItem alloc] init];
    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
    [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    editMenuItem.submenu = editMenu;
    [mainMenu addItem:editMenuItem];

    // View menu
    NSMenuItem* viewMenuItem = [[NSMenuItem alloc] init];
    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    NSMenuItem* tabsItem = [viewMenu addItemWithTitle:@"Tabs" action:@selector(showTabsPanel:) keyEquivalent:@"1"];
    tabsItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    NSMenuItem* bookmarksItem = [viewMenu addItemWithTitle:@"Bookmarks" action:@selector(showBookmarksPanel:) keyEquivalent:@"B"];
    bookmarksItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    NSMenuItem* historyItem = [viewMenu addItemWithTitle:@"History" action:@selector(showHistoryPanel:) keyEquivalent:@"Y"];
    historyItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    NSMenuItem* downloadsItem = [viewMenu addItemWithTitle:@"Downloads" action:@selector(showDownloadsPanel:) keyEquivalent:@"J"];
    downloadsItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [viewMenu addItem:[NSMenuItem separatorItem]];
    [viewMenu addItemWithTitle:@"Reload" action:@selector(reload:) keyEquivalent:@"r"];
    viewMenuItem.submenu = viewMenu;
    [mainMenu addItem:viewMenuItem];

    // History menu
    NSMenuItem* historyMenuItem = [[NSMenuItem alloc] init];
    NSMenu* historyMenu = [[NSMenu alloc] initWithTitle:@"History"];
    [historyMenu addItemWithTitle:@"Show History" action:@selector(showHistoryPanel:) keyEquivalent:@"y"];
    [historyMenu addItem:[NSMenuItem separatorItem]];
    [historyMenu addItemWithTitle:@"Back" action:@selector(goBack:) keyEquivalent:@"["];
    [historyMenu addItemWithTitle:@"Forward" action:@selector(goForward:) keyEquivalent:@"]"];
    [historyMenu addItem:[NSMenuItem separatorItem]];
    // Dynamic history items will be added below this separator
    historyMenu.delegate = self;
    _historyMenu = historyMenu;
    historyMenuItem.submenu = historyMenu;
    [mainMenu addItem:historyMenuItem];

    // Bookmarks menu
    NSMenuItem* bookmarksMenuItem = [[NSMenuItem alloc] init];
    NSMenu* bookmarksMenu = [[NSMenu alloc] initWithTitle:@"Bookmarks"];
    [bookmarksMenu addItemWithTitle:@"Bookmark This Page" action:@selector(bookmarkThisPage:) keyEquivalent:@"d"];
    NSMenuItem* newFolderItem = [bookmarksMenu addItemWithTitle:@"New Folder..." action:@selector(newBookmarkFolder:) keyEquivalent:@"N"];
    newFolderItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;

    // Import Bookmarks submenu
    NSMenuItem* importItem = [[NSMenuItem alloc] initWithTitle:@"Import Bookmarks" action:nil keyEquivalent:@""];
    NSMenu* importMenu = [[NSMenu alloc] init];
    auto detectedBrowsers = BookmarkImporter::DetectBrowsers();
    if (detectedBrowsers.empty()) {
        NSMenuItem* noBrowsers = [[NSMenuItem alloc] initWithTitle:@"No Browsers Detected" action:nil keyEquivalent:@""];
        noBrowsers.enabled = NO;
        [importMenu addItem:noBrowsers];
    } else {
        for (const auto& browser : detectedBrowsers) {
            NSString* title = [NSString stringWithFormat:@"%s (%s)",
                browser.name.c_str(), browser.profile_name.c_str()];
            NSMenuItem* browserItem = [[NSMenuItem alloc] initWithTitle:title
                                                                 action:@selector(importFromBrowser:)
                                                          keyEquivalent:@""];
            browserItem.target = self;
            // Store browser info for the action
            NSDictionary* info = @{
                @"type": @(static_cast<int>(browser.type)),
                @"path": [NSString stringWithUTF8String:browser.profile_path.c_str()],
                @"name": [NSString stringWithUTF8String:browser.name.c_str()]
            };
            browserItem.representedObject = info;
            [importMenu addItem:browserItem];
        }
    }
    importItem.submenu = importMenu;
    [bookmarksMenu addItem:importItem];

    [bookmarksMenu addItem:[NSMenuItem separatorItem]];
    [bookmarksMenu addItemWithTitle:@"Show Bookmarks" action:@selector(showBookmarksPanel:) keyEquivalent:@"b"];
    [bookmarksMenu addItem:[NSMenuItem separatorItem]];
    // Dynamic bookmark items will be added below this separator
    bookmarksMenu.delegate = self;
    _bookmarksMenu = bookmarksMenu;
    bookmarksMenuItem.submenu = bookmarksMenu;
    [mainMenu addItem:bookmarksMenuItem];

    // Window menu
    NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];
    windowMenuItem.submenu = windowMenu;
    [mainMenu addItem:windowMenuItem];

    // Help menu
    NSMenuItem* helpMenuItem = [[NSMenuItem alloc] init];
    NSMenu* helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
    helpMenuItem.submenu = helpMenu;
    [mainMenu addItem:helpMenuItem];

    [NSApp setMainMenu:mainMenu];
    [NSApp setWindowsMenu:windowMenu];
}

// Menu action handlers - forwarded to key window's controller
- (void)newTab:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(createNewTab:)]) {
        [window.windowController performSelector:@selector(createNewTab:) withObject:@""];
    }
}

- (void)closeTab:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(closeCurrentTab)]) {
        [window.windowController performSelector:@selector(closeCurrentTab)];
    }
}

- (void)showTabsPanel:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(showTabsPanel)]) {
        [window.windowController performSelector:@selector(showTabsPanel)];
    }
}

- (void)showHistoryPanel:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(showHistoryPanel)]) {
        [window.windowController performSelector:@selector(showHistoryPanel)];
    }
}

- (void)showBookmarksPanel:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(showBookmarksPanel)]) {
        [window.windowController performSelector:@selector(showBookmarksPanel)];
    }
}

- (void)bookmarkThisPage:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(bookmarkThisPage)]) {
        [window.windowController performSelector:@selector(bookmarkThisPage)];
    }
}

- (void)newBookmarkFolder:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(newBookmarkFolder)]) {
        [window.windowController performSelector:@selector(newBookmarkFolder)];
    }
}

- (void)importFromBrowser:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    NSDictionary* info = item.representedObject;
    if (!info) return;

    BrowserType type = static_cast<BrowserType>([info[@"type"] intValue]);
    NSString* path = info[@"path"];
    NSString* name = info[@"name"];

    DetectedBrowser browser;
    browser.type = type;
    browser.profile_path = [path UTF8String];
    browser.name = [name UTF8String];

    // Import into a folder named after the browser
    ImportResult result = BookmarkImporter::Import(browser, browser.name);

    // Show result alert
    NSAlert* alert = [[NSAlert alloc] init];
    if (result.success) {
        alert.messageText = @"Import Complete";
        alert.informativeText = [NSString stringWithFormat:@"Imported %d bookmarks.\nSkipped %d duplicates.",
            result.imported_count, result.skipped_count];
        alert.alertStyle = NSAlertStyleInformational;
    } else {
        alert.messageText = @"Import Failed";
        alert.informativeText = [NSString stringWithUTF8String:result.error_message.c_str()];
        alert.alertStyle = NSAlertStyleWarning;
    }
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)showDownloadsPanel:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(showDownloadsPanel)]) {
        [window.windowController performSelector:@selector(showDownloadsPanel)];
    }
}

- (void)goBack:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(goBack)]) {
        [window.windowController performSelector:@selector(goBack)];
    }
}

- (void)goForward:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(goForward)]) {
        [window.windowController performSelector:@selector(goForward)];
    }
}

- (void)reload:(id)sender {
    (void)sender;
    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(reload)]) {
        [window.windowController performSelector:@selector(reload)];
    }
}

#pragma mark - NSMenuDelegate

- (void)menuNeedsUpdate:(NSMenu*)menu {
    if (menu == _bookmarksMenu) {
        [self populateBookmarksMenu:menu];
    } else if (menu == _historyMenu) {
        [self populateHistoryMenu:menu];
    }
}

- (void)populateBookmarksMenu:(NSMenu*)menu {
    // Remove dynamic items (keep first 6: Bookmark Page, New Folder, Import Bookmarks, separator, Show Bookmarks, separator)
    while (menu.numberOfItems > 6) {
        [menu removeItemAtIndex:6];
    }

    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (!bookmarks) return;

    auto allBookmarks = bookmarks->GetAllBookmarks();
    auto folders = bookmarks->GetFolders();

    if (allBookmarks.empty() && folders.empty()) {
        NSMenuItem* emptyItem = [[NSMenuItem alloc] initWithTitle:@"No Bookmarks" action:nil keyEquivalent:@""];
        emptyItem.enabled = NO;
        [menu addItem:emptyItem];
        return;
    }

    // Add folders as submenus
    for (const auto& folder : folders) {
        NSMenuItem* folderItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:folder.c_str()]
                                                            action:nil
                                                     keyEquivalent:@""];
        NSMenu* folderMenu = [[NSMenu alloc] init];

        auto folderBookmarks = bookmarks->GetBookmarksInFolder(folder);
        if (folderBookmarks.empty()) {
            NSMenuItem* emptyItem = [[NSMenuItem alloc] initWithTitle:@"Empty Folder" action:nil keyEquivalent:@""];
            emptyItem.enabled = NO;
            [folderMenu addItem:emptyItem];
        } else {
            for (const auto& bm : folderBookmarks) {
                NSString* title = [NSString stringWithUTF8String:bm.title.c_str()];
                if (title.length == 0) {
                    title = [NSString stringWithUTF8String:bm.url.c_str()];
                }
                // Truncate long titles
                if (title.length > 40) {
                    title = [[title substringToIndex:37] stringByAppendingString:@"..."];
                }
                NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                              action:@selector(openBookmarkUrl:)
                                                       keyEquivalent:@""];
                item.representedObject = [NSString stringWithUTF8String:bm.url.c_str()];
                item.target = self;
                [folderMenu addItem:item];
            }
        }

        folderItem.submenu = folderMenu;
        [menu addItem:folderItem];
    }

    // Add root bookmarks (no folder)
    auto rootBookmarks = bookmarks->GetBookmarksInFolder("");
    if (!rootBookmarks.empty() && !folders.empty()) {
        [menu addItem:[NSMenuItem separatorItem]];
    }

    for (const auto& bm : rootBookmarks) {
        NSString* title = [NSString stringWithUTF8String:bm.title.c_str()];
        if (title.length == 0) {
            title = [NSString stringWithUTF8String:bm.url.c_str()];
        }
        // Truncate long titles
        if (title.length > 40) {
            title = [[title substringToIndex:37] stringByAppendingString:@"..."];
        }
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(openBookmarkUrl:)
                                               keyEquivalent:@""];
        item.representedObject = [NSString stringWithUTF8String:bm.url.c_str()];
        item.target = self;
        [menu addItem:item];
    }
}

- (void)populateHistoryMenu:(NSMenu*)menu {
    // Remove dynamic items (keep first 5: Show History, separator, Back, Forward, separator)
    while (menu.numberOfItems > 5) {
        [menu removeItemAtIndex:5];
    }

    HistoryStorage* history = GetHistoryStorage();
    if (!history) return;

    auto entries = history->GetRecentHistory(15);  // Show last 15 entries

    if (entries.empty()) {
        NSMenuItem* emptyItem = [[NSMenuItem alloc] initWithTitle:@"No History" action:nil keyEquivalent:@""];
        emptyItem.enabled = NO;
        [menu addItem:emptyItem];
        return;
    }

    for (const auto& entry : entries) {
        NSString* title = [NSString stringWithUTF8String:entry.title.c_str()];
        if (title.length == 0) {
            title = [NSString stringWithUTF8String:entry.url.c_str()];
        }
        // Truncate long titles
        if (title.length > 50) {
            title = [[title substringToIndex:47] stringByAppendingString:@"..."];
        }
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(openHistoryUrl:)
                                               keyEquivalent:@""];
        item.representedObject = [NSString stringWithUTF8String:entry.url.c_str()];
        item.target = self;
        [menu addItem:item];
    }
}

- (void)openBookmarkUrl:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    NSString* url = item.representedObject;
    if (!url) return;

    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(createNewTab:)]) {
        [window.windowController performSelector:@selector(createNewTab:) withObject:url];
    }
}

- (void)openHistoryUrl:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    NSString* url = item.representedObject;
    if (!url) return;

    NSWindow* window = [NSApp keyWindow];
    if (window.windowController && [window.windowController respondsToSelector:@selector(createNewTab:)]) {
        [window.windowController performSelector:@selector(createNewTab:) withObject:url];
    }
}

@end

// Subclass NSApplication to handle CEF message loop integration
@interface BrowserApplication : NSApplication <CefAppProtocol> {
@private
    BOOL handlingSendEvent_;
}
@end

@implementation BrowserApplication

- (BOOL)isHandlingSendEvent {
    return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
    handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
    CefScopedSendingEvent sendingEventScoper;
    [super sendEvent:event];
}

// Required for proper CEF message loop handling
- (void)terminate:(id)sender {
    (void)sender;
    // Find the main window controller and trigger shutdown directly.
    // Don't rely on keyWindow — it's nil when the app is in the background.
    for (NSWindow* window in [self windows]) {
        if ([window.windowController isKindOfClass:[MainWindowController class]]) {
            [(MainWindowController*)window.windowController shutdownAndQuit];
            return;
        }
    }
    // No main window found — safe to quit directly
    CefQuitMessageLoop();
}

@end

int main(int argc, char* argv[]) {
    // Load the CEF framework library at runtime (must be outside autoreleasepool)
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInMain()) {
        NSLog(@"Failed to load CEF framework");
        return 1;
    }

    // Provide CEF with command-line arguments
    CefMainArgs main_args(argc, argv);

    @autoreleasepool {
        // Initialize the NSApplication
        [BrowserApplication sharedApplication];

        // CEF settings
        CefSettings settings;
        settings.no_sandbox = true;  // Required for development without code signing

#ifdef DEBUG
        // Only enable remote debugging in debug builds
        settings.remote_debugging_port = 9222;
#endif

        // Set cache paths for persistent storage (cookies, localStorage, etc.)
        NSString* appSupportPath = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
        NSString* orbfoxPath = [appSupportPath stringByAppendingPathComponent:@"OrbFox"];
        NSString* cachePath = [orbfoxPath stringByAppendingPathComponent:@"cache"];
        [[NSFileManager defaultManager] createDirectoryAtPath:cachePath withIntermediateDirectories:YES attributes:nil error:nil];
        CefString(&settings.root_cache_path) = [orbfoxPath UTF8String];
        CefString(&settings.cache_path) = [cachePath UTF8String];

        // Set framework path - helps CEF find its resources
        NSString* frameworkPath = [[NSBundle mainBundle] privateFrameworksPath];
        NSString* cefPath = [frameworkPath stringByAppendingPathComponent:@"Chromium Embedded Framework.framework"];
        CefString(&settings.framework_dir_path) = [cefPath UTF8String];

        // Set resources path
        NSString* resourcesPath = [cefPath stringByAppendingPathComponent:@"Resources"];
        CefString(&settings.resources_dir_path) = [resourcesPath UTF8String];

        // Set locales path
        NSString* localesPath = [resourcesPath stringByAppendingPathComponent:@"locales"];
        CefString(&settings.locales_dir_path) = [localesPath UTF8String];

        // Set the main helper app path (CEF will derive the others)
        NSString* helperPath = [frameworkPath stringByAppendingPathComponent:@"OrbFox Helper.app/Contents/MacOS/OrbFox Helper"];
        CefString(&settings.browser_subprocess_path) = [helperPath UTF8String];

        // Create the browser application handler
        CefRefPtr<BrowserApp> app(new BrowserApp);

        // Initialize CEF - may return false if initialization fails
        if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
            NSLog(@"Failed to initialize CEF");
            return 1;
        }

        // Set up the application delegate
        AppDelegate* delegate = [[AppDelegate alloc] init];
        NSApp.delegate = delegate;

        // Activate the application
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];

        // Run the CEF message loop
        // This will block until CefQuitMessageLoop() is called
        CefRunMessageLoop();

        // Session is already saved by shutdownAndQuit/windowShouldClose.
        // Use _exit() to terminate immediately — CefShutdown() crashes if
        // browsers haven't fully completed their async close cycle.
        _exit(0);
    }

    return 0;
}
