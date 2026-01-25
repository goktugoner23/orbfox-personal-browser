#import <Cocoa/Cocoa.h>

#include "browser_app.h"

#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"

// Application delegate
@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    // CEF is initialized in main(), browser is created in OnContextInitialized
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    return NSTerminateNow;
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
