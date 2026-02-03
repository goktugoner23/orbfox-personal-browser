#import <Cocoa/Cocoa.h>

#include "browser_app.h"
#include "tab_manager.h"
#include "window_settings.h"
#include "history_storage.h"
#include "bookmark_storage.h"
#include "session_storage.h"
#include "settings_storage.h"
#include "orbfox_scheme_handler.h"

#import "MainWindowController.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

// Global references (owned by the app)
static std::unique_ptr<TabManager> g_tab_manager;
static std::unique_ptr<HistoryStorage> g_history_storage;
static std::unique_ptr<BookmarkStorage> g_bookmark_storage;
static std::unique_ptr<SessionStorage> g_session_storage;
static MainWindowController* g_window_controller = nil;

// Access global history storage
HistoryStorage* GetHistoryStorage() {
    return g_history_storage.get();
}

// Access global bookmark storage
BookmarkStorage* GetBookmarkStorage() {
    return g_bookmark_storage.get();
}

// Save current session
void SaveSession() {
    if (!g_tab_manager || !g_session_storage) return;

    SavedSession session;
    session.active_workspace_index = 0;

    const auto& workspaces = g_tab_manager->GetWorkspaces();
    Workspace* activeWorkspace = g_tab_manager->GetActiveWorkspace();

    for (size_t i = 0; i < workspaces.size(); ++i) {
        const auto& ws = workspaces[i];
        SavedWorkspace savedWs;
        savedWs.name = ws->name;
        savedWs.color = ws->color;
        savedWs.active_tab_index = ws->active_tab_index;

        if (activeWorkspace && ws->id == activeWorkspace->id) {
            session.active_workspace_index = static_cast<int>(i);
        }

        for (const auto& tab : ws->tabs) {
            SavedTab savedTab;
            savedTab.url = tab->url;
            savedTab.title = tab->title;
            savedTab.is_pinned = tab->is_pinned;
            savedWs.tabs.push_back(savedTab);
        }

        session.workspaces.push_back(savedWs);
    }

    g_session_storage->Save(session);
}

BrowserApp::BrowserApp() = default;

void BrowserApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    // Register "orbfox" as a custom scheme with standard scheme privileges
    // This allows orbfox:// URLs to work like http:// URLs
    registrar->AddCustomScheme(
        "orbfox",
        CEF_SCHEME_OPTION_STANDARD |
        CEF_SCHEME_OPTION_SECURE |
        CEF_SCHEME_OPTION_CORS_ENABLED |
        CEF_SCHEME_OPTION_FETCH_ENABLED
    );
}

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

    // Load application settings
    SettingsStorage::GetInstance().Load();

    // Register orbfox:// custom scheme handler
    RegisterOrbfoxSchemeHandler();

    // Initialize history storage
    g_history_storage = std::make_unique<HistoryStorage>();
    g_history_storage->Initialize();

    // Initialize bookmark storage
    g_bookmark_storage = std::make_unique<BookmarkStorage>();
    g_bookmark_storage->Initialize();

    // Initialize session storage
    g_session_storage = std::make_unique<SessionStorage>();

    // Check if we crashed last session
    bool didCrash = SessionStorage::DidCrashLastSession();

    // Mark browser as running (for crash detection on next startup)
    SessionStorage::MarkRunning();

    // Create tab manager (creates default "WS 1" workspace)
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

    // Try to restore session, otherwise create default tab
    // Always restore on crash, otherwise check settings
    bool sessionRestored = false;
    const Settings& settings = SettingsStorage::GetInstance().Get();
    bool shouldRestore = (didCrash || settings.restore_session) && g_session_storage->HasSavedSession();
    if (shouldRestore) {
        SavedSession session = g_session_storage->Load();

        // Write crash report if we crashed
        if (didCrash && !session.workspaces.empty()) {
            SessionStorage::WriteCrashReport(session);
        }
        if (!session.workspaces.empty()) {
            // Remember the default workspace ID to delete it after creating restored ones
            // (DeleteWorkspace won't delete if it's the only workspace)
            int defaultWorkspaceId = -1;
            if (!g_tab_manager->GetWorkspaces().empty()) {
                defaultWorkspaceId = g_tab_manager->GetWorkspaces()[0]->id;
            }

            // Restore workspaces and tabs
            for (size_t wi = 0; wi < session.workspaces.size(); ++wi) {
                const auto& savedWs = session.workspaces[wi];
                // For auto-generated names (WS N pattern), let CreateWorkspace generate fresh names
                // to ensure proper numbering starting from WS 1
                std::string wsName = savedWs.name;
                if (wsName.rfind("WS ", 0) == 0) {
                    wsName = "";  // Let CreateWorkspace generate a unique name
                }
                Workspace* ws = g_tab_manager->CreateWorkspace(wsName);
                if (!savedWs.color.empty()) {
                    ws->color = savedWs.color;
                }

                if (static_cast<int>(wi) == session.active_workspace_index) {
                    g_tab_manager->SetActiveWorkspace(ws->id);
                }

                // Create tabs in this workspace
                for (const auto& savedTab : savedWs.tabs) {
                    // Temporarily switch to this workspace to create tab in it
                    int currentWsId = g_tab_manager->GetActiveWorkspace() ? g_tab_manager->GetActiveWorkspace()->id : ws->id;
                    g_tab_manager->SetActiveWorkspace(ws->id);

                    Tab* tab = g_tab_manager->CreateTab(savedTab.url);
                    if (tab) {
                        tab->title = savedTab.title;
                        tab->is_pinned = savedTab.is_pinned;
                    }

                    g_tab_manager->SetActiveWorkspace(currentWsId);
                }

                // Set active tab index
                if (savedWs.active_tab_index >= 0 && savedWs.active_tab_index < static_cast<int>(ws->tabs.size())) {
                    ws->active_tab_index = savedWs.active_tab_index;
                }
            }

            // Delete the default workspace now that we have restored workspaces
            if (defaultWorkspaceId >= 0) {
                g_tab_manager->DeleteWorkspace(defaultWorkspaceId);
            }

            // Renumber auto-generated workspace names (WS N) starting from 1
            // Also reassign colors based on index to ensure variety
            int wsIndex = 0;
            for (auto& ws : g_tab_manager->GetWorkspaces()) {
                if (ws->name.rfind("WS ", 0) == 0) {
                    ws->name = "WS " + std::to_string(wsIndex + 1);
                }
                ws->color = WorkspaceColors::ForIndex(wsIndex);
                wsIndex++;
            }

            // Switch to the saved active workspace
            if (session.active_workspace_index >= 0 &&
                session.active_workspace_index < static_cast<int>(g_tab_manager->GetWorkspaces().size())) {
                g_tab_manager->SetActiveWorkspace(
                    g_tab_manager->GetWorkspaces()[session.active_workspace_index]->id);
            }

            sessionRestored = true;
        }
    }

    // If no session restored, create default tab
    if (!sessionRestored) {
        g_tab_manager->CreateTab(settings.new_tab_url);
    }

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

    // Save session when window is about to close
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                                                      object:window
                                                       queue:nil
                                                  usingBlock:^(NSNotification* note) {
        (void)note;
        SaveSession();
        // Mark clean shutdown so we know we didn't crash
        SessionStorage::MarkCleanShutdown();
    }];
}
