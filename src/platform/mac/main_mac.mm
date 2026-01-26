#import <Cocoa/Cocoa.h>

#include "browser_app.h"

#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"

// Forward declarations for menu actions
@class MainWindowController;

// Application delegate
@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void)setupMenuBar;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    // CEF is initialized in main(), browser is created in OnContextInitialized
    [self setupMenuBar];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
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
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Personal Browser"];
    [appMenu addItemWithTitle:@"About Personal Browser" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide Personal Browser" action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem* hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Personal Browser" action:@selector(terminate:) keyEquivalent:@"q"];
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
    historyMenuItem.submenu = historyMenu;
    [mainMenu addItem:historyMenuItem];

    // Bookmarks menu
    NSMenuItem* bookmarksMenuItem = [[NSMenuItem alloc] init];
    NSMenu* bookmarksMenu = [[NSMenu alloc] initWithTitle:@"Bookmarks"];
    [bookmarksMenu addItemWithTitle:@"Show Bookmarks" action:@selector(showBookmarksPanel:) keyEquivalent:@"b"];
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

        // Set a unique cache path to avoid singleton conflicts
        NSString* appSupportPath = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
        NSString* cachePath = [appSupportPath stringByAppendingPathComponent:@"PersonalBrowser"];
        [[NSFileManager defaultManager] createDirectoryAtPath:cachePath withIntermediateDirectories:YES attributes:nil error:nil];
        CefString(&settings.root_cache_path) = [cachePath UTF8String];

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
        NSString* helperPath = [frameworkPath stringByAppendingPathComponent:@"Personal Browser Helper.app/Contents/MacOS/Personal Browser Helper"];
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

        // Shutdown CEF
        CefShutdown();
    }

    return 0;
}
