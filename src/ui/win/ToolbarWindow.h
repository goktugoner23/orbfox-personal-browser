#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <functional>

class TabManager;

// Toolbar window for OrbFox on Windows
// Contains navigation buttons and URL bar (at the bottom like macOS version)
class ToolbarWindow {
public:
    enum class NavigationAction {
        Back,
        Forward,
        Reload
    };

    explicit ToolbarWindow(TabManager* tab_manager);
    ~ToolbarWindow();

    // Create the toolbar as a child of the main window
    bool Create(HWND parent);

    // Get window handle
    HWND GetHWND() const { return hwnd_; }

    // Update URL bar
    void SetUrl(const std::string& url);
    std::string GetUrl() const;

    // Update button states
    void SetCanGoBack(bool can);
    void SetCanGoForward(bool can);
    void SetLoading(bool loading);

    // Focus URL bar
    void FocusUrlBar();

    // Update blocked tracker count
    void SetBlockedCount(int count);

    // Avatar / Account
    void SetSignedIn(bool signed_in);
    void SetAvatarImage(HBITMAP bitmap);
    void SetAvatarUrl(const std::string& url);

    // Callbacks
    using NavigationCallback = std::function<void(NavigationAction)>;
    using UrlSubmitCallback = std::function<void(const std::string& url)>;
    using AvatarClickCallback = std::function<void()>;

    void SetNavigationCallback(NavigationCallback callback) { on_navigation_ = std::move(callback); }
    void SetUrlSubmitCallback(UrlSubmitCallback callback) { on_url_submit_ = std::move(callback); }
    void SetAvatarClickCallback(AvatarClickCallback callback) { on_avatar_click_ = std::move(callback); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK UrlEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Register window class
    static bool RegisterWindowClass(HINSTANCE hInstance);

    // Message handlers
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnMouseLeave();

    // Drawing
    void DrawButtons(HDC hdc);
    void DrawSecurityIndicator(HDC hdc);
    void DrawAvatarButton(HDC hdc);

    // Layout update
    void UpdateLayout();

    // Process URL input
    void OnUrlSubmit();

    // Window handles
    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND url_edit_ = nullptr;

    // Original URL edit window procedure
    WNDPROC original_url_edit_proc_ = nullptr;

    // Tab management
    TabManager* tab_manager_ = nullptr;

    // Button state
    bool can_go_back_ = false;
    bool can_go_forward_ = false;
    bool is_loading_ = false;

    // Hover state
    int hovered_button_ = -1;  // 0=back, 1=forward, 2=reload, 3=avatar
    bool tracking_mouse_ = false;
    int blocked_count_ = 0;

    // Avatar state
    bool is_signed_in_ = false;
    HBITMAP avatar_bitmap_ = nullptr;
    std::string avatar_url_;

    // Layout
    static constexpr int kButtonSize = 28;
    static constexpr int kButtonSpacing = 4;
    static constexpr int kUrlBarHeight = 32;
    static constexpr int kSidebarWidth = 280;  // Must match MainWindow
    static constexpr int kAvatarSize = 28;     // Circular avatar size

    // Callbacks
    NavigationCallback on_navigation_;
    UrlSubmitCallback on_url_submit_;
    AvatarClickCallback on_avatar_click_;

    // Fonts
    HFONT font_ = nullptr;

    // Window class name
    static constexpr wchar_t kWindowClassName[] = L"OrbFoxToolbar";
    static bool class_registered_;

    // Command IDs
    static constexpr int ID_URL_EDIT = 1001;
};

#endif  // PLATFORM_WIN
