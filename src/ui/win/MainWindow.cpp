#ifdef PLATFORM_WIN

#include "MainWindow.h"
#include "SidebarWindow.h"
#include "ToolbarWindow.h"
#include "FindBar.h"
#include "DesignSystem.h"
#include "GestureHandler.h"
#include "DevToolsPanel.h"
#include "AutocompleteDropdown.h"
#include "tab_manager.h"
#include "browser_client.h"
#include "window_settings.h"
#include "session_storage.h"
#include "settings_storage.h"
#include "bookmark_storage.h"
#include "utils/filesystem_utils.h"

#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"

#include <windowsx.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <commctrl.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

// Defined in browser_app_win.cpp
extern void SaveSession();
extern BookmarkStorage* GetBookmarkStorage();

bool MainWindow::class_registered_ = false;

MainWindow::MainWindow(TabManager* tab_manager)
    : tab_manager_(tab_manager) {
}

MainWindow::~MainWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool MainWindow::RegisterWindowClass(HINSTANCE hInstance) {
    if (class_registered_) {
        return true;
    }

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(MainWindow*);
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(DesignSystem::GetBackgroundColor());
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = kWindowClassName;
    wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

    if (!RegisterClassExW(&wcex)) {
        return false;
    }

    class_registered_ = true;
    return true;
}

bool MainWindow::Create() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!RegisterWindowClass(hInstance)) {
        return false;
    }

    // Load saved window settings
    WindowSettings settings = WindowSettings::Load();
    int x = settings.x > 0 ? settings.x : CW_USEDEFAULT;
    int y = settings.y > 0 ? settings.y : CW_USEDEFAULT;
    int width = settings.width > 0 ? settings.width : kDefaultWidth;
    int height = settings.height > 0 ? settings.height : kDefaultHeight;

    // Create main window
    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"OrbFox",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y,
        width, height,
        nullptr,
        nullptr,
        hInstance,
        this  // Pass this pointer for WM_CREATE
    );

    if (!hwnd_) {
        return false;
    }

    // Enable dark mode for title bar (Windows 10 1809+)
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    // Maximize if saved that way
    if (settings.maximized) {
        ShowWindow(hwnd_, SW_MAXIMIZE);
    }

    return true;
}

void MainWindow::Show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }
}

void MainWindow::Close() {
    if (hwnd_) {
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    }
}

int MainWindow::GetWidth() const {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    return rc.right - rc.left;
}

int MainWindow::GetHeight() const {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    return rc.bottom - rc.top;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_CLOSE:
            OnClose();
            return 0;

        case WM_DESTROY:
            OnDestroy();
            return 0;

        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;

        case WM_KEYDOWN:
            OnKeyDown(wParam, lParam);
            return 0;

        // Custom message from BrowserClient::OnPreKeyEvent forwarding shortcuts
        case (WM_APP + 1):
            OnShortcut(wParam, lParam);
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = kMinWidth;
            mmi->ptMinTrackSize.y = kMinHeight;
            return 0;
        }

        case WM_ERASEBKGND:
            // Prevent flickering
            return 1;

        case WM_TIMER:
            if (wParam == kHibernationTimerId && tab_manager_) {
                tab_manager_->HibernateInactiveTabs(300);  // 5 minutes, matches macOS
            }
            return 0;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void MainWindow::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Create browser parent window (container for CEF browser views)
    browser_parent_hwnd_ = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );

    // Create sidebar
    sidebar_ = std::make_unique<SidebarWindow>(tab_manager_);
    sidebar_->Create(hwnd_);

    // Create toolbar
    toolbar_ = std::make_unique<ToolbarWindow>(tab_manager_);
    toolbar_->Create(hwnd_);

    // Create find bar
    find_bar_ = std::make_unique<FindBar>();
    find_bar_->Create(hwnd_);
    find_bar_->SetFindCallback([this](const std::string& text, bool forward, bool match_case) {
        if (tab_manager_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->browser) {
                if (text.empty()) {
                    active->browser->GetHost()->StopFinding(true);
                } else {
                    active->browser->GetHost()->Find(text, forward, match_case, false);
                }
            }
        }
    });
    find_bar_->SetCloseCallback([this]() {
        if (tab_manager_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->browser) {
                active->browser->GetHost()->StopFinding(true);
            }
        }
    });

    // Set up toolbar callbacks
    toolbar_->SetNavigationCallback([this](ToolbarWindow::NavigationAction action) {
        switch (action) {
            case ToolbarWindow::NavigationAction::Back:
                GoBack();
                break;
            case ToolbarWindow::NavigationAction::Forward:
                GoForward();
                break;
            case ToolbarWindow::NavigationAction::Reload:
                if (tab_manager_) {
                    Tab* active = tab_manager_->GetActiveTab();
                    if (active && active->browser) {
                        if (active->browser->IsLoading()) {
                            active->browser->StopLoad();
                        } else {
                            active->browser->Reload();
                        }
                    }
                }
                break;
        }
    });

    toolbar_->SetUrlSubmitCallback([this](const std::string& url) {
        if (tab_manager_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->browser) {
                active->browser->GetMainFrame()->LoadURL(url);
            }
        }
    });

    // Set up sidebar callbacks
    sidebar_->SetTabSelectedCallback([this](int tab_id) {
        if (tab_manager_) {
            tab_manager_->SetActiveTab(tab_id);
        }
    });

    sidebar_->SetNewTabCallback([this]() {
        CreateNewTab();
    });

    sidebar_->SetTabCloseCallback([this](int tab_id) {
        if (tab_manager_) {
            Tab* tab = tab_manager_->GetTabById(tab_id);
            if (tab && tab->browser) {
                tab->browser->GetHost()->CloseBrowser(false);
            }
            tab_manager_->CloseTab(tab_id);
        }
    });

    sidebar_->SetTabDuplicateCallback([this](int tab_id) {
        if (tab_manager_) {
            Tab* tab = tab_manager_->GetTabById(tab_id);
            if (tab) {
                Tab* newTab = tab_manager_->CreateTab(tab->url);
                if (newTab) {
                    tab_manager_->SetActiveTab(newTab->id);
                }
            }
        }
    });

    sidebar_->SetWorkspaceSelectedCallback([this](int workspace_id) {
        if (tab_manager_) {
            tab_manager_->SetActiveWorkspace(workspace_id);
        }
    });

    sidebar_->SetNewWorkspaceCallback([this]() {
        if (tab_manager_) {
            tab_manager_->CreateWorkspace("New Workspace");
        }
    });

    sidebar_->SetOpenUrlCallback([this](const std::string& url, bool background) {
        if (tab_manager_) {
            Tab* newTab = tab_manager_->CreateTab(url);
            if (newTab && !background) {
                tab_manager_->SetActiveTab(newTab->id);
            }
        }
    });

    // Create gesture handler and attach to browser area
    gesture_handler_ = std::make_unique<GestureHandler>();
    gesture_handler_->Attach(browser_parent_hwnd_);
    gesture_handler_->SetGestureCallback([this](GestureType gesture) {
        switch (gesture) {
            case GestureType::Left:
                GoBack();
                break;
            case GestureType::Right:
                GoForward();
                break;
            case GestureType::LShape:
                CloseCurrentTab();
                break;
            case GestureType::ReverseLShape:
                if (tab_manager_) {
                    tab_manager_->ReopenClosedTab();
                }
                break;
            default:
                break;
        }
    });

    // Create DevTools panel
    devtools_panel_ = std::make_unique<DevToolsPanel>();
    devtools_panel_->Create(browser_parent_hwnd_);
    devtools_panel_->SetCloseCallback([this]() {
        UpdateLayout();
    });
    devtools_panel_->SetResizeCallback([this](int newBrowserWidth) {
        // Resize browser views so DevTools doesn't overlap page content
        if (!tab_manager_) return;
        RECT browserRect;
        GetClientRect(browser_parent_hwnd_, &browserRect);
        int height = browserRect.bottom;

        for (const auto& ws : tab_manager_->GetWorkspaces()) {
            for (const auto& tab : ws->tabs) {
                if (tab->browser) {
                    HWND browserHwnd = tab->browser->GetHost()->GetWindowHandle();
                    if (browserHwnd) {
                        SetWindowPos(browserHwnd, nullptr,
                            0, 0, newBrowserWidth, height,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
            }
        }
    });

    // Set up tab manager callbacks
    if (tab_manager_) {
        TabManagerCallbacks callbacks;

        callbacks.on_tab_activated = [this](Tab* tab) {
            if (!tab) return;

            // Update active time for hibernation tracking
            tab_manager_->UpdateTabActiveTime(tab->id);

            // If tab is hibernated, wake it (triggers on_tab_woken callback)
            if (tab->is_hibernated) {
                tab_manager_->WakeTab(tab->id);
                return;
            }

            // Update URL bar
            if (toolbar_) {
                toolbar_->SetUrl(tab->url);
                toolbar_->SetCanGoBack(tab->browser ? tab->browser->CanGoBack() : false);
                toolbar_->SetCanGoForward(tab->browser ? tab->browser->CanGoForward() : false);
                int blockedCount = tab->client ? tab->client->GetBlockedCount() : 0;
                toolbar_->SetBlockedCount(blockedCount);
            }

            // Update window title
            UpdateWindowTitle(tab->title);

            // Show the browser for this tab, hide others
            ShowBrowserForTab(tab->id);
        };

        callbacks.on_tab_created = [this](Tab* tab) {
            if (tab && !tab->browser) {
                CreateBrowserForTab(tab->id, tab->url);
            }
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        callbacks.on_tab_closed = [this](Tab* tab) {
            if (tab && tab->browser) {
                CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
                if (host) {
                    HWND browserHwnd = host->GetWindowHandle();
                    if (browserHwnd && IsWindow(browserHwnd)) {
                        ShowWindow(browserHwnd, SW_HIDE);
                    }
                    host->CloseBrowser(true);
                }
            }
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        callbacks.on_tab_updated = [this](Tab* /*tab*/) {
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        callbacks.on_workspace_changed = [this](Workspace* /*ws*/) {
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        callbacks.on_tab_hibernated = [this](Tab* tab) {
            if (!tab) return;
            // Close browser to free memory
            if (tab->browser) {
                CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
                if (host) {
                    HWND browserHwnd = host->GetWindowHandle();
                    if (browserHwnd && IsWindow(browserHwnd)) {
                        ShowWindow(browserHwnd, SW_HIDE);
                    }
                    host->CloseBrowser(true);
                }
            }
            tab->browser = nullptr;
            tab->client = nullptr;
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        callbacks.on_tab_woken = [this](Tab* tab) {
            // Recreate browser for woken tab
            if (tab && !tab->browser) {
                CreateBrowserForTab(tab->id, tab->url);
            }
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
        };

        tab_manager_->SetCallbacks(callbacks);
    }

    // Start hibernation timer — check every 60 seconds (matches macOS)
    SetTimer(hwnd_, kHibernationTimerId, 60000, nullptr);

    UpdateLayout();
}

void MainWindow::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
}

void MainWindow::OnClose() {
    KillTimer(hwnd_, kHibernationTimerId);

    // Save window settings
    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd_, &wp);

    WindowSettings settings;
    settings.x = wp.rcNormalPosition.left;
    settings.y = wp.rcNormalPosition.top;
    settings.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    settings.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    settings.maximized = (wp.showCmd == SW_MAXIMIZE);
    settings.Save();

    // Save session and mark clean shutdown (matches macOS behavior)
    SaveSession();
    SessionStorage::MarkCleanShutdown();

    // Close all browsers
    if (tab_manager_) {
        for (const auto& ws : tab_manager_->GetWorkspaces()) {
            for (const auto& tab : ws->tabs) {
                if (tab->browser) {
                    tab->browser->GetHost()->CloseBrowser(true);
                }
            }
        }
    }

    DestroyWindow(hwnd_);
}

void MainWindow::OnDestroy() {
    CefQuitMessageLoop();
    PostQuitMessage(0);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    // Menu/accelerator command handling
    int cmd = LOWORD(wParam);
    switch (cmd) {
        // TODO: Add menu command IDs
        default:
            break;
    }
}

void MainWindow::OnKeyDown(WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (ctrl && !alt) {
        switch (wParam) {
            case 'T':
                if (shift) {
                    // Ctrl+Shift+T: Reopen closed tab
                    if (tab_manager_) {
                        tab_manager_->ReopenClosedTab();
                    }
                } else {
                    CreateNewTab();
                }
                break;
            case 'W':
                CloseCurrentTab();
                break;
            case 'L':
                FocusUrlBar();
                break;
            case 'R':
                Reload();
                break;
            case 'F':
                ShowFindBar();
                break;
            case 0xDB:  // VK_OEM_4 = '[' — Ctrl+[: Back
                GoBack();
                break;
            case 0xDD:  // VK_OEM_6 = ']' — Ctrl+]: Forward
                GoForward();
                break;
        }
    }

    // Ctrl+Alt shortcuts (matching Cmd+Option on macOS)
    if (ctrl && alt) {
        switch (wParam) {
            case 'I':
                ToggleDevTools();
                break;
            case VK_LEFT:
                SwitchToPreviousWorkspace();
                break;
            case VK_RIGHT:
                SwitchToNextWorkspace();
                break;
        }
    }

    // Ctrl+1-9: Switch to tab by index
    if (ctrl && !shift && !alt && wParam >= '1' && wParam <= '9') {
        int index = static_cast<int>(wParam - '1');
        if (tab_manager_) {
            Workspace* ws = tab_manager_->GetActiveWorkspace();
            if (ws && index < static_cast<int>(ws->tabs.size())) {
                tab_manager_->SetActiveTab(ws->tabs[index]->id);
            }
        }
    }

    // F12: DevTools
    if (wParam == VK_F12) {
        ToggleDevTools();
    }

    // Escape: hide find bar
    if (wParam == VK_ESCAPE) {
        if (find_bar_ && find_bar_->IsVisible()) {
            HideFindBar();
        }
    }
}

void MainWindow::OnShortcut(WPARAM wParam, LPARAM lParam) {
    // Handle shortcuts forwarded from BrowserClient::OnPreKeyEvent
    // Shortcut IDs match those defined in resource.h
    switch (wParam) {
        case 1:  // SHORTCUT_NEW_TAB
            CreateNewTab();
            break;
        case 2:  // SHORTCUT_CLOSE_TAB
            CloseCurrentTab();
            break;
        case 3:  // SHORTCUT_FIND
            ShowFindBar();
            break;
        case 4:  // SHORTCUT_REOPEN_TAB
            if (tab_manager_) {
                tab_manager_->ReopenClosedTab();
            }
            break;
        case 5:  // SHORTCUT_TOGGLE_DEVTOOLS
            ToggleDevTools();
            break;
        case 6:  // SHORTCUT_PREV_WORKSPACE
            SwitchToPreviousWorkspace();
            break;
        case 7:  // SHORTCUT_NEXT_WORKSPACE
            SwitchToNextWorkspace();
            break;
        case 8:  // SHORTCUT_SWITCH_TAB
        {
            int index = static_cast<int>(lParam);
            if (tab_manager_) {
                Workspace* ws = tab_manager_->GetActiveWorkspace();
                if (ws && index >= 0 && index < static_cast<int>(ws->tabs.size())) {
                    tab_manager_->SetActiveTab(ws->tabs[index]->id);
                }
            }
            break;
        }
        default:
            break;
    }
}

void MainWindow::SwitchToPreviousWorkspace() {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspaces.size() <= 1) return;

    Workspace* active = tab_manager_->GetActiveWorkspace();
    if (!active) return;

    for (size_t i = 0; i < workspaces.size(); ++i) {
        if (workspaces[i]->id == active->id) {
            int prevIndex = (i == 0) ? static_cast<int>(workspaces.size()) - 1 : static_cast<int>(i) - 1;
            tab_manager_->SetActiveWorkspace(workspaces[prevIndex]->id);
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
            break;
        }
    }
}

void MainWindow::SwitchToNextWorkspace() {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspaces.size() <= 1) return;

    Workspace* active = tab_manager_->GetActiveWorkspace();
    if (!active) return;

    for (size_t i = 0; i < workspaces.size(); ++i) {
        if (workspaces[i]->id == active->id) {
            int nextIndex = (i + 1 >= workspaces.size()) ? 0 : static_cast<int>(i) + 1;
            tab_manager_->SetActiveWorkspace(workspaces[nextIndex]->id);
            if (sidebar_) {
                sidebar_->RefreshTabList();
            }
            break;
        }
    }
}

void MainWindow::UpdateLayout() {
    if (!hwnd_) return;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    // FindBar at top of browser area (if visible)
    int findBarHeight = 0;
    if (find_bar_ && find_bar_->IsVisible()) {
        findBarHeight = 36;
        SetWindowPos(find_bar_->GetHWND(), nullptr,
            kSidebarWidth, 0,
            width - kSidebarWidth, findBarHeight,
            SWP_NOZORDER);
    }

    // Sidebar on left
    if (sidebar_ && sidebar_->GetHWND()) {
        SetWindowPos(sidebar_->GetHWND(), nullptr,
            0, 0,
            kSidebarWidth, height - kToolbarHeight,
            SWP_NOZORDER);
    }

    // Browser area (right of sidebar, above toolbar, below find bar)
    if (browser_parent_hwnd_) {
        SetWindowPos(browser_parent_hwnd_, nullptr,
            kSidebarWidth, findBarHeight,
            width - kSidebarWidth, height - kToolbarHeight - findBarHeight,
            SWP_NOZORDER);
    }

    // Toolbar at bottom
    if (toolbar_ && toolbar_->GetHWND()) {
        SetWindowPos(toolbar_->GetHWND(), nullptr,
            0, height - kToolbarHeight,
            width, kToolbarHeight,
            SWP_NOZORDER);
    }

    // Update DevTools position if visible
    if (devtools_panel_ && devtools_panel_->IsVisible() && !devtools_panel_->IsAnimating()) {
        devtools_panel_->UpdatePosition();
    }

    // Resize all browser views to fit browser area (minus DevTools if open)
    if (tab_manager_) {
        RECT browserRect;
        GetClientRect(browser_parent_hwnd_, &browserRect);

        int browserWidth = browserRect.right;
        if (devtools_panel_ && devtools_panel_->IsVisible() && !devtools_panel_->IsAnimating()) {
            browserWidth = browserRect.right - devtools_panel_->GetWidth() - 5;  // 5 = divider
        }

        for (const auto& ws : tab_manager_->GetWorkspaces()) {
            for (const auto& tab : ws->tabs) {
                if (tab->browser) {
                    HWND browserHwnd = tab->browser->GetHost()->GetWindowHandle();
                    if (browserHwnd) {
                        SetWindowPos(browserHwnd, nullptr,
                            0, 0,
                            browserWidth, browserRect.bottom,
                            SWP_NOZORDER);
                    }
                }
            }
        }
    }
}

void MainWindow::CreateBrowserForTab(int tab_id, const std::string& url) {
    if (!browser_parent_hwnd_) return;

    Tab* tab = tab_manager_ ? tab_manager_->GetTabById(tab_id) : nullptr;
    if (!tab) return;

    // Create browser client with callbacks
    CefRefPtr<BrowserClient> client = new BrowserClient();
    tab->client = client;

    client->SetTitleChangeCallback([this, tab_id](const std::string& title) {
        if (tab_manager_) {
            tab_manager_->UpdateTabTitle(tab_id, title);
            // Update window title if this is the active tab
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id) {
                UpdateWindowTitle(title);
            }
        }
    });

    client->SetAddressChangeCallback([this, tab_id](const std::string& address) {
        if (tab_manager_) {
            tab_manager_->UpdateTabUrl(tab_id, address);
            // Update URL bar if this is the active tab
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id && toolbar_) {
                toolbar_->SetUrl(address);
            }
        }
    });

    client->SetLoadingStateCallback([this, tab_id](bool isLoading, bool canGoBack, bool canGoForward) {
        if (tab_manager_) {
            tab_manager_->UpdateTabLoadingState(tab_id, isLoading);
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id && toolbar_) {
                toolbar_->SetLoading(isLoading);
                toolbar_->SetCanGoBack(canGoBack);
                toolbar_->SetCanGoForward(canGoForward);
            }
        }
    });

    client->SetBrowserCreatedCallback([this, tab_id](CefRefPtr<CefBrowser> browser) {
        if (tab_manager_) {
            Tab* t = tab_manager_->GetTabById(tab_id);
            if (t) {
                t->browser = browser;
            }
        }
        if (on_browser_created_) {
            on_browser_created_(browser);
        }
    });

    client->SetPopupRequestCallback([this](const std::string& popup_url) {
        CreateNewTab(popup_url);
    });

    client->SetOpenLinkCallback([this](const std::string& link_url, bool background) {
        if (tab_manager_) {
            Tab* newTab = tab_manager_->CreateTab(link_url);
            if (newTab && !background) {
                tab_manager_->SetActiveTab(newTab->id);
            }
        }
    });

    client->SetFindResultCallback([this, tab_id](int count, int activeMatch) {
        // Only update find bar if this is the active tab
        if (tab_manager_ && find_bar_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id) {
                find_bar_->SetMatchCount(count, activeMatch);
            }
        }
    });

    client->SetFaviconChangeCallback([this, tab_id](const std::string& url, const std::vector<unsigned char>& png_data) {
        if (tab_manager_) {
            tab_manager_->UpdateTabFavicon(tab_id, url, png_data);
        }
    });

    client->SetBlockedCountCallback([this, tab_id](int count) {
        if (tab_manager_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id && toolbar_) {
                toolbar_->SetBlockedCount(count);
            }
        }
    });

    client->SetCopyToClipboardCallback([this](const std::string& text) {
        CopyToClipboard(text);
    });

    client->SetFocusUrlBarCallback([this]() {
        FocusUrlBar();
    });

    client->SetInspectElementCallback([this](int x, int y) {
        ShowDevToolsAtPoint(x, y);
    });

    client->SetFullscreenChangeCallback([this, tab_id](bool fullscreen) {
        if (tab_manager_) {
            Tab* active = tab_manager_->GetActiveTab();
            if (active && active->id == tab_id) {
                SetContentFullscreen(fullscreen);
            }
        }
    });

    client->SetBookmarkActionCallback([this](const std::string& action, const std::string& param) {
        if (action == "add") {
            // Bookmark current page
            Tab* active = tab_manager_ ? tab_manager_->GetActiveTab() : nullptr;
            if (active) {
                BookmarkStorage* storage = GetBookmarkStorage();
                if (storage) {
                    storage->AddBookmark(active->title, active->url, 0);
                    if (sidebar_) {
                        sidebar_->RefreshTabList();
                    }
                }
            }
        } else if (action == "add_link") {
            // Bookmark a specific link from context menu
            size_t tabPos = param.find('\t');
            std::string url = (tabPos != std::string::npos) ? param.substr(0, tabPos) : param;
            std::string title = (tabPos != std::string::npos) ? param.substr(tabPos + 1) : url;
            BookmarkStorage* storage = GetBookmarkStorage();
            if (storage) {
                storage->AddBookmark(title, url, 0);
            }
        }
    });

    client->SetDownloadDialogCallback([this](const std::string& suggested_name,
                                             int64_t total_bytes,
                                             CefRefPtr<CefBeforeDownloadCallback> callback) {
        std::string filename = suggested_name.empty() ? "download" : suggested_name;

        // Format size string
        std::wstring sizeStr;
        if (total_bytes < 0) {
            sizeStr = L"Unknown size";
        } else if (total_bytes < 1024) {
            sizeStr = std::to_wstring(total_bytes) + L" bytes";
        } else if (total_bytes < 1024 * 1024) {
            wchar_t buf[32];
            swprintf(buf, 32, L"%.1f KB", total_bytes / 1024.0);
            sizeStr = buf;
        } else {
            wchar_t buf[32];
            swprintf(buf, 32, L"%.1f MB", total_bytes / (1024.0 * 1024.0));
            sizeStr = buf;
        }

        std::wstring content = L"Size: " + sizeStr;
        std::wstring mainText = L"Download \"" + DesignSystem::Utf8ToWide(filename) + L"\"";

        // Custom buttons: Save, Save As, Cancel
        const int ID_SAVE = 100;
        const int ID_SAVE_AS = 101;
        TASKDIALOG_BUTTON buttons[] = {
            { ID_SAVE, L"Save" },
            { ID_SAVE_AS, L"Save As\u2026" },
        };

        TASKDIALOGCONFIG tdc = {};
        tdc.cbSize = sizeof(tdc);
        tdc.hwndParent = hwnd_;
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pszWindowTitle = L"Download File";
        tdc.pszMainInstruction = mainText.c_str();
        tdc.pszContent = content.c_str();
        tdc.cButtons = 2;
        tdc.pButtons = buttons;
        tdc.nDefaultButton = ID_SAVE;
        tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;

        int clicked = 0;
        TaskDialogIndirect(&tdc, &clicked, nullptr, nullptr);

        if (clicked == ID_SAVE) {
            std::string download_path = orbfox::utils::GetHomeDirectory() + "\\Downloads\\" + filename;
            callback->Continue(download_path, false);
        } else if (clicked == ID_SAVE_AS) {
            OPENFILENAMEW ofn = {};
            wchar_t szFile[MAX_PATH] = {};
            wcscpy_s(szFile, DesignSystem::Utf8ToWide(filename).c_str());

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd_;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"All Files\0*.*\0";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

            if (GetSaveFileNameW(&ofn)) {
                std::wstring wpath(szFile);
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string path(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &path[0], size_needed, nullptr, nullptr);
                callback->Continue(path, false);
            }
            // If cancelled, download is simply not started
        }
        // IDCANCEL = do nothing, download is cancelled
    });

    client->SetNavigationStartCallback([this, tab_id]() {
        // Navigation started — favicon will be updated by OnFaviconURLChange
        if (tab_manager_) {
            tab_manager_->UpdateTabLoadingState(tab_id, true);
        }
    });

    // Get browser parent rect
    RECT rect;
    GetClientRect(browser_parent_hwnd_, &rect);

    // Browser window info
    CefWindowInfo window_info;
    window_info.SetAsChild(browser_parent_hwnd_, rect);

    // Browser settings
    CefBrowserSettings browser_settings;

    // Initial URL — use settings new_tab_url for empty URLs (matches macOS)
    std::string load_url = url.empty() ? SettingsStorage::GetInstance().Get().new_tab_url : url;
    CefBrowserHost::CreateBrowser(window_info, client, load_url, browser_settings, nullptr, nullptr);
}

void MainWindow::ShowBrowserForTab(int tab_id) {
    if (!tab_manager_ || !browser_parent_hwnd_) return;

    // Hide all browsers, show only the active one
    for (const auto& ws : tab_manager_->GetWorkspaces()) {
        for (const auto& tab : ws->tabs) {
            if (tab->browser) {
                HWND browserHwnd = tab->browser->GetHost()->GetWindowHandle();
                if (browserHwnd) {
                    ShowWindow(browserHwnd, (tab->id == tab_id) ? SW_SHOW : SW_HIDE);
                }
            }
        }
    }
}

void MainWindow::CreateNewTab(const std::string& url) {
    if (tab_manager_) {
        Tab* tab = tab_manager_->CreateTab(url);
        if (tab) {
            tab_manager_->SetActiveTab(tab->id);
        }
    }
}

void MainWindow::CloseCurrentTab() {
    if (tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active) {
            if (active->browser) {
                active->browser->GetHost()->CloseBrowser(false);
            }
            tab_manager_->CloseTab(active->id);
        }
    }
}

void MainWindow::GoBack() {
    if (tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active && active->browser && active->browser->CanGoBack()) {
            active->browser->GoBack();
        }
    }
}

void MainWindow::GoForward() {
    if (tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active && active->browser && active->browser->CanGoForward()) {
            active->browser->GoForward();
        }
    }
}

void MainWindow::Reload() {
    if (tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active && active->browser) {
            active->browser->Reload();
        }
    }
}

void MainWindow::FocusUrlBar() {
    if (toolbar_) {
        toolbar_->FocusUrlBar();
    }
}

void MainWindow::ShowTabsPanel() {
    if (sidebar_) {
        sidebar_->ShowPanel(SidebarWindow::Panel::Tabs);
    }
}

void MainWindow::ShowBookmarksPanel() {
    if (sidebar_) {
        sidebar_->ShowPanel(SidebarWindow::Panel::Bookmarks);
    }
}

void MainWindow::ShowHistoryPanel() {
    if (sidebar_) {
        sidebar_->ShowPanel(SidebarWindow::Panel::History);
    }
}

void MainWindow::ShowDownloadsPanel() {
    if (sidebar_) {
        sidebar_->ShowPanel(SidebarWindow::Panel::Downloads);
    }
}

void MainWindow::ShowFindBar() {
    if (find_bar_) {
        find_bar_->Show();
        UpdateLayout();
    }
}

void MainWindow::HideFindBar() {
    if (find_bar_) {
        find_bar_->Hide();
        UpdateLayout();
    }
}

void MainWindow::ShowDevTools() {
    ShowDevToolsAtPoint(-1, -1);
}

void MainWindow::ShowDevToolsAtPoint(int x, int y) {
    if (devtools_panel_ && tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active && active->browser) {
            devtools_panel_->Show(active->browser, x, y);
        }
    }
}

void MainWindow::HideDevTools() {
    if (devtools_panel_) {
        devtools_panel_->Hide();
    }
}

void MainWindow::ToggleDevTools() {
    if (devtools_panel_) {
        if (devtools_panel_->IsVisible()) {
            HideDevTools();
        } else {
            ShowDevTools();
        }
    }
}

void MainWindow::SetContentFullscreen(bool fullscreen) {
    if (fullscreen == is_content_fullscreen_) return;
    is_content_fullscreen_ = fullscreen;

    // Track state in the active tab's BrowserClient for keyboard filtering
    if (tab_manager_) {
        Tab* active = tab_manager_->GetActiveTab();
        if (active && active->browser) {
            // BrowserClient tracks this for modifier key suppression in fullscreen
            CefRefPtr<CefClient> client = active->browser->GetHost()->GetClient();
            BrowserClient* bc = static_cast<BrowserClient*>(client.get());
            if (bc) {
                bc->SetContentFullscreen(fullscreen);
            }
        }
    }

    if (fullscreen) {
        // Save current window state
        saved_style_ = GetWindowLong(hwnd_, GWL_STYLE);
        saved_ex_style_ = GetWindowLong(hwnd_, GWL_EXSTYLE);
        GetWindowRect(hwnd_, &saved_rect_);

        // Remove window chrome and maximize to monitor
        SetWindowLong(hwnd_, GWL_STYLE, saved_style_ & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLong(hwnd_, GWL_EXSTYLE, saved_ex_style_ & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);

        SetWindowPos(hwnd_, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);

        // Close DevTools if open (matches macOS)
        HideDevTools();

        // Hide find bar if visible (matches macOS)
        HideFindBar();

        // Hide sidebar and toolbar in content fullscreen
        if (sidebar_ && sidebar_->GetHWND()) {
            ShowWindow(sidebar_->GetHWND(), SW_HIDE);
        }
        if (toolbar_ && toolbar_->GetHWND()) {
            ShowWindow(toolbar_->GetHWND(), SW_HIDE);
        }

        // Browser fills entire window
        if (browser_parent_hwnd_) {
            RECT rc;
            GetClientRect(hwnd_, &rc);
            SetWindowPos(browser_parent_hwnd_, nullptr,
                0, 0, rc.right, rc.bottom,
                SWP_NOZORDER);
        }
    } else {
        // Restore window state
        SetWindowLong(hwnd_, GWL_STYLE, saved_style_);
        SetWindowLong(hwnd_, GWL_EXSTYLE, saved_ex_style_);

        SetWindowPos(hwnd_, nullptr,
            saved_rect_.left, saved_rect_.top,
            saved_rect_.right - saved_rect_.left,
            saved_rect_.bottom - saved_rect_.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);

        // Show sidebar and toolbar again
        if (sidebar_ && sidebar_->GetHWND()) {
            ShowWindow(sidebar_->GetHWND(), SW_SHOW);
        }
        if (toolbar_ && toolbar_->GetHWND()) {
            ShowWindow(toolbar_->GetHWND(), SW_SHOW);
        }

        UpdateLayout();
    }
}

void MainWindow::UpdateWindowTitle(const std::string& pageTitle) {
    if (!hwnd_) return;

    std::wstring title;
    if (pageTitle.empty()) {
        title = L"OrbFox";
    } else {
        title = DesignSystem::Utf8ToWide(pageTitle) + L" - OrbFox";
    }
    SetWindowTextW(hwnd_, title.c_str());
}

void MainWindow::CopyToClipboard(const std::string& text) {
    if (text.empty()) return;

    std::wstring wtext = DesignSystem::Utf8ToWide(text);

    if (OpenClipboard(hwnd_)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
            if (pMem) {
                wcscpy_s(pMem, wtext.size() + 1, wtext.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }
}

#endif  // PLATFORM_WIN
