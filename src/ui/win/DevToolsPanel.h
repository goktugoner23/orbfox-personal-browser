#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <functional>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

// Forward declaration
class DevToolsClientWin;

// DevTools panel for OrbFox on Windows
// Slides in from the right side of the browser area with a draggable divider
class DevToolsPanel {
public:
    DevToolsPanel();
    ~DevToolsPanel();

    // Create the panel as a child of the browser parent container
    bool Create(HWND parent);

    // Get window handles
    HWND GetHWND() const { return hwnd_; }
    HWND GetDividerHWND() const { return divider_hwnd_; }

    // Show DevTools for the given browser (optionally at a specific point for "Inspect Element")
    void Show(CefRefPtr<CefBrowser> browser, int inspectX = -1, int inspectY = -1);

    // Hide and close DevTools
    void Hide();

    // Toggle DevTools visibility for the given browser
    void Toggle(CefRefPtr<CefBrowser> browser);

    // Check if visible
    bool IsVisible() const;

    // Check if the given browser is currently being inspected
    bool IsInspecting(CefRefPtr<CefBrowser> browser) const;

    // Check if currently animating
    bool IsAnimating() const { return animating_; }

    // Get/set panel width
    int GetWidth() const { return panel_width_; }
    void SetWidth(int width);

    // Resize to a new width (called during divider drag)
    void ResizeToWidth(int newWidth);

    // Update position after parent resize
    void UpdatePosition();

    // Callbacks
    using CloseCallback = std::function<void()>;
    using ResizeCallback = std::function<void(int newBrowserWidth)>;

    void SetCloseCallback(CloseCallback cb) { on_close_ = std::move(cb); }
    void SetResizeCallback(ResizeCallback cb) { on_resize_ = std::move(cb); }

    // Static window procedures
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK DividerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Register window classes
    static bool RegisterWindowClasses(HINSTANCE hInstance);

    // Message handlers for panel
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseLeave();

    // Message handlers for divider
    LRESULT HandleDividerMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnDividerPaint();
    void OnDividerMouseMove(int x, int y);
    void OnDividerLButtonDown(int x, int y);
    void OnDividerLButtonUp(int x, int y);
    void OnDividerMouseEnter();
    void OnDividerMouseLeave();

    // Layout update
    void UpdateLayout();

    // Animation helpers
    void AnimateIn();
    void AnimateOut();
    void OnAnimationTick();
    static double EaseOut(double t);
    static double EaseIn(double t);

    // Window handles
    HWND hwnd_ = nullptr;           // Main panel window
    HWND divider_hwnd_ = nullptr;   // Divider handle for resizing
    HWND parent_ = nullptr;         // Browser parent container
    HWND header_bar_ = nullptr;     // Header bar at top
    HWND close_button_ = nullptr;   // Close button
    HWND title_label_ = nullptr;    // "DevTools" label
    HWND browser_container_ = nullptr;  // Container for CEF browser

    // CEF browser for DevTools
    CefRefPtr<CefBrowser> devtools_browser_;
    CefRefPtr<CefBrowser> inspected_browser_;  // The browser being inspected

    // State
    bool visible_ = false;
    bool animating_ = false;
    bool divider_dragging_ = false;
    bool divider_hovered_ = false;
    bool close_button_hovered_ = false;
    bool tracking_mouse_ = false;
    bool tracking_divider_mouse_ = false;
    int drag_start_x_ = 0;
    int drag_start_width_ = 0;

    // Animation
    bool anim_opening_ = false;  // true = animating in, false = animating out
    DWORD anim_start_tick_ = 0;
    int anim_start_x_ = 0;
    int anim_end_x_ = 0;
    int anim_divider_start_x_ = 0;
    int anim_divider_end_x_ = 0;
    static constexpr UINT_PTR kAnimTimerId = 1;
    static constexpr int kAnimDurationMs = 220;  // Match macOS 0.22s
    static constexpr int kAnimFrameMs = 16;      // ~60fps

    // Layout
    int panel_width_ = 420;  // Default width

    // Callbacks
    CloseCallback on_close_;
    ResizeCallback on_resize_;

    // Fonts
    HFONT font_normal_ = nullptr;
    HFONT font_bold_ = nullptr;

    // Layout constants
    static constexpr int kHeaderHeight = 28;
    static constexpr int kDividerWidth = 5;
    static constexpr int kMinWidth = 280;
    static constexpr int kMaxWidth = 800;
    static constexpr int kDefaultWidth = 420;

    // Control IDs
    static constexpr int ID_CLOSE = 1001;

    // Window class names
    static constexpr wchar_t kClassName[] = L"OrbFoxDevToolsPanel";
    static constexpr wchar_t kDividerClassName[] = L"OrbFoxDevToolsDivider";
    static bool class_registered_;
};

// Minimal CefClient for DevTools panel (Windows version)
// Named differently to avoid conflict with macOS DevToolsClient
class DevToolsClientWin : public CefClient,
                          public CefLifeSpanHandler,
                          public CefLoadHandler {
public:
    DevToolsClientWin() = default;

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
        if (on_browser_created_) {
            on_browser_created_(browser);
        }
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        browser_ = nullptr;
    }

    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override {
        (void)httpStatusCode;
        (void)browser;
        if (!frame->IsMain()) return;
        // DevTools loaded - no JavaScript injection needed
    }

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

    using BrowserCreatedCallback = std::function<void(CefRefPtr<CefBrowser>)>;
    void SetBrowserCreatedCallback(BrowserCreatedCallback cb) { on_browser_created_ = std::move(cb); }

private:
    CefRefPtr<CefBrowser> browser_;
    BrowserCreatedCallback on_browser_created_;

    IMPLEMENT_REFCOUNTING(DevToolsClientWin);
    DISALLOW_COPY_AND_ASSIGN(DevToolsClientWin);
};

#endif  // PLATFORM_WIN
