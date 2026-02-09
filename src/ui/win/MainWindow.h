#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <memory>
#include <functional>

#include "include/cef_browser.h"

class TabManager;
class SidebarWindow;
class ToolbarWindow;
class FindBar;
class GestureHandler;
class DevToolsPanel;
class AutocompleteDropdown;

// Main window for OrbFox on Windows
// Hosts the sidebar, content area (CEF browser), and bottom toolbar
class MainWindow {
public:
    explicit MainWindow(TabManager* tab_manager);
    ~MainWindow();

    // Create and show the window
    bool Create();
    void Show();
    void Close();

    // Get window handle
    HWND GetHWND() const { return hwnd_; }

    // Get content area handle (where CEF browser renders)
    HWND GetBrowserParentHWND() const { return browser_parent_hwnd_; }

    // Window dimensions
    int GetWidth() const;
    int GetHeight() const;

    // Tab/navigation operations (called from menu or keyboard shortcuts)
    void CreateNewTab(const std::string& url = "");
    void CloseCurrentTab();
    void GoBack();
    void GoForward();
    void Reload();
    void FocusUrlBar();

    // Panel operations
    void ShowTabsPanel();
    void ShowBookmarksPanel();
    void ShowHistoryPanel();
    void ShowDownloadsPanel();

    // Workspace switching
    void SwitchToPreviousWorkspace();
    void SwitchToNextWorkspace();

    // Find in page
    void ShowFindBar();
    void HideFindBar();

    // Developer tools
    void ShowDevTools();
    void ShowDevToolsAtPoint(int x, int y);
    void HideDevTools();
    void ToggleDevTools();

    // Content fullscreen (HTML5 Fullscreen API, e.g. YouTube)
    void SetContentFullscreen(bool fullscreen);

    // Window title
    void UpdateWindowTitle(const std::string& pageTitle);

    // Clipboard
    void CopyToClipboard(const std::string& text);

    // Callbacks for CEF browser events
    using BrowserCreatedCallback = std::function<void(CefRefPtr<CefBrowser>)>;
    void SetBrowserCreatedCallback(BrowserCreatedCallback callback) { on_browser_created_ = std::move(callback); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Register window class
    static bool RegisterWindowClass(HINSTANCE hInstance);

    // Message handlers
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnSize(int width, int height);
    void OnClose();
    void OnDestroy();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnKeyDown(WPARAM wParam, LPARAM lParam);
    void OnShortcut(WPARAM wParam, LPARAM lParam);

    // Layout
    void UpdateLayout();

    // Create CEF browser for a tab
    void CreateBrowserForTab(int tab_id, const std::string& url);

    // Show browser for the specified tab, hide others
    void ShowBrowserForTab(int tab_id);

    // Window handles
    HWND hwnd_ = nullptr;
    HWND browser_parent_hwnd_ = nullptr;  // Container for CEF browser views

    // Child windows
    std::unique_ptr<SidebarWindow> sidebar_;
    std::unique_ptr<ToolbarWindow> toolbar_;
    std::unique_ptr<FindBar> find_bar_;
    std::unique_ptr<GestureHandler> gesture_handler_;
    std::unique_ptr<DevToolsPanel> devtools_panel_;

    // Tab management
    TabManager* tab_manager_ = nullptr;

    // Callbacks
    BrowserCreatedCallback on_browser_created_;

    // Content fullscreen state
    bool is_content_fullscreen_ = false;
    LONG saved_style_ = 0;
    LONG saved_ex_style_ = 0;
    RECT saved_rect_ = {};

    // Menu bar
    void CreateMenuBar();

    // Timer IDs
    static constexpr UINT_PTR kHibernationTimerId = 1;

    // Menu command IDs
    enum MenuCmd {
        IDM_FILE_NEW_TAB = 40001,
        IDM_FILE_CLOSE_TAB,
        IDM_FILE_REOPEN_TAB,
        IDM_FILE_EXIT,
        IDM_EDIT_FIND,
        IDM_VIEW_RELOAD,
        IDM_VIEW_STOP,
        IDM_VIEW_DEVTOOLS,
        IDM_VIEW_FULLSCREEN,
        IDM_WINDOW_TABS,
        IDM_WINDOW_BOOKMARKS,
        IDM_WINDOW_HISTORY,
        IDM_WINDOW_DOWNLOADS,
        IDM_WINDOW_SETTINGS,
        IDM_WINDOW_PREV_WORKSPACE,
        IDM_WINDOW_NEXT_WORKSPACE,
        IDM_HELP_ABOUT,
    };

    // Layout constants
    static constexpr int kDefaultWidth = 1280;
    static constexpr int kDefaultHeight = 800;
    static constexpr int kMinWidth = 800;
    static constexpr int kMinHeight = 600;
    static constexpr int kSidebarWidth = 280;
    static constexpr int kToolbarHeight = 48;

    // Window class name
    static constexpr wchar_t kWindowClassName[] = L"OrbFoxMainWindow";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
