#import <Cocoa/Cocoa.h>

#include "browser_app.h"
#include "tab_manager.h"
#include "window_settings.h"

#import "MainWindowController.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

// Global references (owned by the app)
static std::unique_ptr<TabManager> g_tab_manager;
static MainWindowController* g_window_controller = nil;

BrowserApp::BrowserApp() = default;

void BrowserApp::OnBeforeCommandLineProcessing(
    const CefString& /*process_type*/,
    CefRefPtr<CefCommandLine> /*command_line*/) {
    // Multi-process mode is now enabled via helper apps
    // No special flags needed for normal operation
}

void BrowserApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

    // Load window settings
    WindowSettings window_settings = WindowSettings::Load();

    // Create tab manager
    g_tab_manager = std::make_unique<TabManager>();

    // Create the main window controller with native UI
    g_window_controller = [[MainWindowController alloc] initWithTabManager:g_tab_manager.get()];

    // Configure window with saved settings
    NSWindow* window = g_window_controller.window;
    NSRect frame = NSMakeRect(window_settings.x, window_settings.y,
                               window_settings.width, window_settings.height);
    [window setFrame:frame display:NO];

    if (window_settings.maximized) {
        [window zoom:nil];
    }

    // Show the window
    [window makeKeyAndOrderFront:nil];

    // Create initial tab
    g_tab_manager->CreateTab("https://www.google.com");

    // Set up window bounds change notification for persistence
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidResizeNotification
                                                      object:window
                                                       queue:nil
                                                  usingBlock:^(NSNotification* note) {
        (void)note;
        NSWindow* win = g_window_controller.window;
        if (!win.isZoomed && !win.isMiniaturized) {
            NSRect frame = win.frame;
            WindowSettings settings = WindowSettings::Load();
            settings.x = static_cast<int>(frame.origin.x);
            settings.y = static_cast<int>(frame.origin.y);
            settings.width = static_cast<int>(frame.size.width);
            settings.height = static_cast<int>(frame.size.height);
            settings.maximized = false;
            settings.Save();
        }
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidMoveNotification
                                                      object:window
                                                       queue:nil
                                                  usingBlock:^(NSNotification* note) {
        (void)note;
        NSWindow* win = g_window_controller.window;
        if (!win.isZoomed && !win.isMiniaturized) {
            NSRect frame = win.frame;
            WindowSettings settings = WindowSettings::Load();
            settings.x = static_cast<int>(frame.origin.x);
            settings.y = static_cast<int>(frame.origin.y);
            settings.width = static_cast<int>(frame.size.width);
            settings.height = static_cast<int>(frame.size.height);
            settings.maximized = false;
            settings.Save();
        }
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEndLiveResizeNotification
                                                      object:window
                                                       queue:nil
                                                  usingBlock:^(NSNotification* note) {
        (void)note;
        NSWindow* win = g_window_controller.window;
        WindowSettings settings = WindowSettings::Load();
        settings.maximized = win.isZoomed;
        if (!win.isZoomed && !win.isMiniaturized) {
            NSRect frame = win.frame;
            settings.x = static_cast<int>(frame.origin.x);
            settings.y = static_cast<int>(frame.origin.y);
            settings.width = static_cast<int>(frame.size.width);
            settings.height = static_cast<int>(frame.size.height);
        }
        settings.Save();
    }];
}
