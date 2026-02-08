#ifdef PLATFORM_WIN

#include "DevToolsPanel.h"
#include "DesignSystem.h"

#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"

#include <windowsx.h>

bool DevToolsPanel::class_registered_ = false;

DevToolsPanel::DevToolsPanel() = default;

DevToolsPanel::~DevToolsPanel() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_bold_) DeleteObject(font_bold_);
    if (divider_hwnd_) DestroyWindow(divider_hwnd_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool DevToolsPanel::RegisterWindowClasses(HINSTANCE hInstance) {
    if (class_registered_) return true;

    // Register panel window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = DevToolsPanel::WndProc;
    wcex.cbWndExtra = sizeof(DevToolsPanel*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = kClassName;

    if (!RegisterClassExW(&wcex)) return false;

    // Register divider window class
    WNDCLASSEXW dividerClass = {};
    dividerClass.cbSize = sizeof(WNDCLASSEXW);
    dividerClass.style = CS_HREDRAW | CS_VREDRAW;
    dividerClass.lpfnWndProc = DevToolsPanel::DividerWndProc;
    dividerClass.cbWndExtra = sizeof(DevToolsPanel*);
    dividerClass.hInstance = hInstance;
    dividerClass.hCursor = LoadCursor(nullptr, IDC_SIZEWE);  // Left-right resize cursor
    dividerClass.hbrBackground = nullptr;
    dividerClass.lpszClassName = kDividerClassName;

    if (!RegisterClassExW(&dividerClass)) return false;

    class_registered_ = true;
    return true;
}

bool DevToolsPanel::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!RegisterWindowClasses(hInstance)) return false;

    // Create the divider (positioned to the left of the panel)
    divider_hwnd_ = CreateWindowExW(
        0,
        kDividerClassName,
        nullptr,
        WS_CHILD | WS_CLIPCHILDREN,  // Not visible initially
        0, 0, kDividerWidth, 0,
        parent,
        nullptr,
        hInstance,
        this
    );

    if (!divider_hwnd_) return false;

    // Create the main panel
    hwnd_ = CreateWindowExW(
        0,
        kClassName,
        nullptr,
        WS_CHILD | WS_CLIPCHILDREN,  // Not visible initially
        0, 0, 0, 0,
        parent,
        nullptr,
        hInstance,
        this
    );

    return hwnd_ != nullptr;
}

LRESULT CALLBACK DevToolsPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DevToolsPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<DevToolsPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<DevToolsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) {
        return panel->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK DevToolsPanel::DividerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DevToolsPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<DevToolsPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->divider_hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<DevToolsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) {
        return panel->HandleDividerMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT DevToolsPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            OnPaint();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;

        case WM_DRAWITEM: {
            // Owner-draw for close button
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == ID_CLOSE) {
                HDC hdc = dis->hDC;
                RECT rc = dis->rcItem;

                // Background
                COLORREF bgColor = (dis->itemState & ODS_SELECTED)
                    ? DesignSystem::GetSurfaceActiveColor()
                    : (close_button_hovered_ ? DesignSystem::GetSurfaceHoverColor() : RGB(32, 33, 36));
                HBRUSH brush = CreateSolidBrush(bgColor);
                FillRect(hdc, &rc, brush);
                DeleteObject(brush);

                // Draw X symbol
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, DesignSystem::GetTextSecondaryColor());
                HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
                DrawTextW(hdc, L"\u2715", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldFont);

                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            // Style the title label
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DesignSystem::GetTextSecondaryColor());
            SetBkColor(hdc, RGB(32, 33, 36));
            static HBRUSH labelBrush = CreateSolidBrush(RGB(32, 33, 36));
            return (LRESULT)labelBrush;
        }

        case WM_ERASEBKGND:
            return 1;  // Prevent flicker

        case WM_TIMER:
            if (wParam == kAnimTimerId) {
                OnAnimationTick();
            }
            return 0;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

LRESULT DevToolsPanel::HandleDividerMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(divider_hwnd_, &ps);
            OnDividerPaint();
            EndPaint(divider_hwnd_, &ps);
            return 0;
        }

        case WM_MOUSEMOVE:
            OnDividerMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDOWN:
            OnDividerLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONUP:
            OnDividerLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEENTER:
            OnDividerMouseEnter();
            return 0;

        case WM_MOUSELEAVE:
            OnDividerMouseLeave();
            return 0;

        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProc(divider_hwnd_, msg, wParam, lParam);
}

void DevToolsPanel::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_bold_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody(), FW_SEMIBOLD);

    // Create header bar (drawn manually in OnPaint)
    // No separate window needed - just reserve space at top

    // Create close button
    close_button_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"\u2715",  // X symbol
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        0, 0, 24, 24,
        hwnd_,
        (HMENU)ID_CLOSE,
        hInstance,
        nullptr
    );
    SendMessage(close_button_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    // Create title label
    title_label_ = CreateWindowExW(
        0,
        L"STATIC",
        L"DevTools",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 0, 80, kHeaderHeight,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );
    SendMessage(title_label_, WM_SETFONT, (WPARAM)font_bold_, TRUE);

    // Create browser container (where CEF browser will be hosted)
    browser_container_ = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, kHeaderHeight, 0, 0,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );
}

void DevToolsPanel::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Double buffering
    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Fill background with DevTools background color (slightly different from main surface)
    COLORREF bgColor = RGB(32, 33, 36);  // Chrome DevTools background
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw header bar
    RECT headerRect = { 0, 0, clientRect.right, kHeaderHeight };
    HBRUSH headerBrush = CreateSolidBrush(bgColor);
    FillRect(memDC, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    // Draw header bottom border
    HPEN borderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, kHeaderHeight - 1, nullptr);
    LineTo(memDC, clientRect.right, kHeaderHeight - 1);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Draw left border (edge of panel)
    HPEN leftBorderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen2 = (HPEN)SelectObject(memDC, leftBorderPen);
    MoveToEx(memDC, 0, 0, nullptr);
    LineTo(memDC, 0, clientRect.bottom);
    SelectObject(memDC, oldPen2);
    DeleteObject(leftBorderPen);

    // Copy to screen
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void DevToolsPanel::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DevToolsPanel::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    int id = LOWORD(wParam);

    if (id == ID_CLOSE) {
        Hide();
    }
}

void DevToolsPanel::OnMouseMove(int x, int y) {
    // Track mouse for WM_MOUSELEAVE
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    // Check if hovering over close button area
    RECT closeRect;
    if (close_button_ && GetWindowRect(close_button_, &closeRect)) {
        MapWindowPoints(HWND_DESKTOP, hwnd_, (LPPOINT)&closeRect, 2);
        POINT pt = { x, y };
        bool wasHovered = close_button_hovered_;
        close_button_hovered_ = PtInRect(&closeRect, pt);
        if (wasHovered != close_button_hovered_) {
            InvalidateRect(close_button_, nullptr, FALSE);
        }
    }
}

void DevToolsPanel::OnLButtonDown(int x, int y) {
    (void)x;
    (void)y;
}

void DevToolsPanel::OnLButtonUp(int x, int y) {
    (void)x;
    (void)y;
}

void DevToolsPanel::OnMouseLeave() {
    tracking_mouse_ = false;
    if (close_button_hovered_) {
        close_button_hovered_ = false;
        InvalidateRect(close_button_, nullptr, FALSE);
    }
}

void DevToolsPanel::OnDividerPaint() {
    RECT clientRect;
    GetClientRect(divider_hwnd_, &clientRect);

    HDC hdc = GetDC(divider_hwnd_);

    // Fill with divider color
    COLORREF dividerColor = divider_hovered_ || divider_dragging_
        ? DesignSystem::GetSurfaceHoverColor()
        : DesignSystem::GetBorderColor();
    HBRUSH brush = CreateSolidBrush(dividerColor);
    FillRect(hdc, &clientRect, brush);
    DeleteObject(brush);

    // Draw grip dots in center
    COLORREF gripColor = DesignSystem::GetTextTertiaryColor();
    int cx = clientRect.right / 2;
    int cy = clientRect.bottom / 2;
    int dotSize = 2;
    int dotSpacing = 5;

    for (int i = -1; i <= 1; i++) {
        RECT dot = {
            cx - dotSize / 2,
            cy + i * dotSpacing - dotSize / 2,
            cx + dotSize / 2 + 1,
            cy + i * dotSpacing + dotSize / 2 + 1
        };
        HBRUSH dotBrush = CreateSolidBrush(gripColor);
        HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
        HPEN oldPen = (HPEN)SelectObject(hdc, nullPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, dotBrush);
        Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(dotBrush);
    }

    ReleaseDC(divider_hwnd_, hdc);
}

void DevToolsPanel::OnDividerMouseMove(int x, int y) {
    (void)y;

    // Track mouse for WM_MOUSELEAVE
    if (!tracking_divider_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = divider_hwnd_;
        TrackMouseEvent(&tme);
        tracking_divider_mouse_ = true;
        OnDividerMouseEnter();
    }

    if (divider_dragging_) {
        // Calculate new width based on mouse position
        // The divider is to the left of the panel, so moving left increases width
        POINT screenPt = { x, 0 };
        ClientToScreen(divider_hwnd_, &screenPt);

        RECT parentRect;
        GetClientRect(parent_, &parentRect);

        POINT parentPt = screenPt;
        ScreenToClient(parent_, &parentPt);

        // New width = distance from mouse to right edge of parent
        int newWidth = parentRect.right - parentPt.x;

        // Clamp to min/max
        newWidth = max(kMinWidth, min(kMaxWidth, newWidth));

        ResizeToWidth(newWidth);
    }
}

void DevToolsPanel::OnDividerLButtonDown(int x, int y) {
    (void)x;
    (void)y;
    divider_dragging_ = true;
    drag_start_width_ = panel_width_;
    SetCapture(divider_hwnd_);
    InvalidateRect(divider_hwnd_, nullptr, FALSE);
}

void DevToolsPanel::OnDividerLButtonUp(int x, int y) {
    (void)x;
    (void)y;
    if (divider_dragging_) {
        divider_dragging_ = false;
        ReleaseCapture();
        InvalidateRect(divider_hwnd_, nullptr, FALSE);
    }
}

void DevToolsPanel::OnDividerMouseEnter() {
    if (!divider_hovered_) {
        divider_hovered_ = true;
        InvalidateRect(divider_hwnd_, nullptr, FALSE);
    }
}

void DevToolsPanel::OnDividerMouseLeave() {
    tracking_divider_mouse_ = false;
    if (divider_hovered_ && !divider_dragging_) {
        divider_hovered_ = false;
        InvalidateRect(divider_hwnd_, nullptr, FALSE);
    }
}

void DevToolsPanel::UpdateLayout() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    // Position close button in top-right of header
    int buttonSize = 24;
    int buttonY = (kHeaderHeight - buttonSize) / 2;
    if (close_button_) {
        SetWindowPos(close_button_, nullptr,
            width - buttonSize - 8, buttonY,
            buttonSize, buttonSize,
            SWP_NOZORDER);
    }

    // Position title label
    if (title_label_) {
        SetWindowPos(title_label_, nullptr,
            10, buttonY,
            80, buttonSize,
            SWP_NOZORDER);
    }

    // Position browser container (below header)
    if (browser_container_) {
        SetWindowPos(browser_container_, nullptr,
            1, kHeaderHeight,  // 1 pixel offset for left border
            width - 1, height - kHeaderHeight,
            SWP_NOZORDER);
    }

    // Resize DevTools browser if it exists
    if (devtools_browser_) {
        HWND browserHwnd = devtools_browser_->GetHost()->GetWindowHandle();
        if (browserHwnd) {
            RECT containerRect;
            GetClientRect(browser_container_, &containerRect);
            SetWindowPos(browserHwnd, nullptr,
                0, 0,
                containerRect.right, containerRect.bottom,
                SWP_NOZORDER);
        }
    }
}

double DevToolsPanel::EaseOut(double t) {
    return 1.0 - (1.0 - t) * (1.0 - t);
}

double DevToolsPanel::EaseIn(double t) {
    return t * t;
}

void DevToolsPanel::OnAnimationTick() {
    DWORD elapsed = GetTickCount() - anim_start_tick_;
    double progress = static_cast<double>(elapsed) / kAnimDurationMs;
    if (progress > 1.0) progress = 1.0;

    double eased = anim_opening_ ? EaseOut(progress) : EaseIn(progress);

    // Interpolate positions
    int panelX = anim_start_x_ + static_cast<int>((anim_end_x_ - anim_start_x_) * eased);
    int dividerX = anim_divider_start_x_ + static_cast<int>((anim_divider_end_x_ - anim_divider_start_x_) * eased);

    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentHeight = parentRect.bottom;
    int currentWidth = parentRect.right - panelX;

    // Move panel
    SetWindowPos(hwnd_, nullptr,
        panelX, 0, currentWidth, parentHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // Move divider
    SetWindowPos(divider_hwnd_, nullptr,
        dividerX, 0, kDividerWidth, parentHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // Notify about browser resize during animation
    if (on_resize_) {
        on_resize_(dividerX);
    }

    if (progress >= 1.0) {
        // Animation complete
        KillTimer(hwnd_, kAnimTimerId);
        animating_ = false;

        if (!anim_opening_) {
            // Slide-out complete — hide and clean up
            ShowWindow(hwnd_, SW_HIDE);
            ShowWindow(divider_hwnd_, SW_HIDE);
            visible_ = false;

            devtools_browser_ = nullptr;
            inspected_browser_ = nullptr;

            if (on_close_) {
                on_close_();
            }
            if (on_resize_) {
                on_resize_(parentRect.right);
            }
        } else {
            UpdateLayout();
        }
    }
}

void DevToolsPanel::AnimateIn() {
    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentWidth = parentRect.right;

    // Panel starts off-screen right, ends at target
    anim_start_x_ = parentWidth;
    anim_end_x_ = parentWidth - panel_width_;

    // Divider starts off-screen, ends at target
    anim_divider_start_x_ = parentWidth;
    anim_divider_end_x_ = parentWidth - panel_width_ - kDividerWidth;

    anim_opening_ = true;
    animating_ = true;
    anim_start_tick_ = GetTickCount();

    SetTimer(hwnd_, kAnimTimerId, kAnimFrameMs, nullptr);
}

void DevToolsPanel::AnimateOut() {
    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentWidth = parentRect.right;

    // Panel starts at current position, ends off-screen right
    anim_start_x_ = parentWidth - panel_width_;
    anim_end_x_ = parentWidth;

    // Divider follows
    anim_divider_start_x_ = parentWidth - panel_width_ - kDividerWidth;
    anim_divider_end_x_ = parentWidth;

    anim_opening_ = false;
    animating_ = true;
    anim_start_tick_ = GetTickCount();

    SetTimer(hwnd_, kAnimTimerId, kAnimFrameMs, nullptr);
}

void DevToolsPanel::Show(CefRefPtr<CefBrowser> browser, int inspectX, int inspectY) {
    if (visible_ || animating_) return;
    if (!browser) return;

    inspected_browser_ = browser;
    visible_ = true;
    panel_width_ = kDefaultWidth;

    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentWidth = parentRect.right;
    int parentHeight = parentRect.bottom;

    // Start off-screen (animation will slide in)
    SetWindowPos(hwnd_, nullptr,
        parentWidth, 0, panel_width_, parentHeight,
        SWP_NOZORDER);
    ShowWindow(hwnd_, SW_SHOW);

    SetWindowPos(divider_hwnd_, nullptr,
        parentWidth, 0, kDividerWidth, parentHeight,
        SWP_NOZORDER);
    ShowWindow(divider_hwnd_, SW_SHOW);

    UpdateLayout();

    // Create DevTools browser
    CefRefPtr<DevToolsClientWin> client = new DevToolsClientWin();
    client->SetBrowserCreatedCallback([this](CefRefPtr<CefBrowser> devBrowser) {
        devtools_browser_ = devBrowser;
        UpdateLayout();
    });

    CefWindowInfo windowInfo;
    RECT containerRect;
    GetClientRect(browser_container_, &containerRect);
    windowInfo.SetAsChild(browser_container_, containerRect);

    CefBrowserSettings settings;

    CefPoint inspectPoint;
    if (inspectX >= 0 && inspectY >= 0) {
        inspectPoint.Set(inspectX, inspectY);
    }

    browser->GetHost()->ShowDevTools(windowInfo, client, settings, inspectPoint);

    // Start slide-in animation
    AnimateIn();
}

void DevToolsPanel::Hide() {
    if (!visible_ || animating_) return;

    // Close DevTools via CEF
    if (inspected_browser_) {
        inspected_browser_->GetHost()->CloseDevTools();
    }

    // Start slide-out animation (cleanup happens in OnAnimationTick when complete)
    AnimateOut();
}

bool DevToolsPanel::IsVisible() const {
    return visible_;
}

void DevToolsPanel::Toggle(CefRefPtr<CefBrowser> browser) {
    if (visible_) {
        Hide();
    } else {
        Show(browser);
    }
}

bool DevToolsPanel::IsInspecting(CefRefPtr<CefBrowser> browser) const {
    if (!visible_ || !browser || !inspected_browser_) {
        return false;
    }
    return inspected_browser_->GetIdentifier() == browser->GetIdentifier();
}

void DevToolsPanel::SetWidth(int width) {
    panel_width_ = max(kMinWidth, min(kMaxWidth, width));
}

void DevToolsPanel::ResizeToWidth(int newWidth) {
    if (!visible_ || animating_) return;

    panel_width_ = max(kMinWidth, min(kMaxWidth, newWidth));

    // Reposition divider and panel
    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentWidth = parentRect.right;
    int parentHeight = parentRect.bottom;

    int dividerX = parentWidth - panel_width_ - kDividerWidth;
    SetWindowPos(divider_hwnd_, nullptr,
        dividerX, 0,
        kDividerWidth, parentHeight,
        SWP_NOZORDER);

    SetWindowPos(hwnd_, nullptr,
        parentWidth - panel_width_, 0,
        panel_width_, parentHeight,
        SWP_NOZORDER);

    UpdateLayout();

    // Notify about resize
    if (on_resize_) {
        on_resize_(dividerX);
    }
}

void DevToolsPanel::UpdatePosition() {
    if (!visible_) return;

    RECT parentRect;
    GetClientRect(parent_, &parentRect);
    int parentWidth = parentRect.right;
    int parentHeight = parentRect.bottom;

    // Reposition divider
    int dividerX = parentWidth - panel_width_ - kDividerWidth;
    SetWindowPos(divider_hwnd_, nullptr,
        dividerX, 0,
        kDividerWidth, parentHeight,
        SWP_NOZORDER);

    // Reposition panel
    SetWindowPos(hwnd_, nullptr,
        parentWidth - panel_width_, 0,
        panel_width_, parentHeight,
        SWP_NOZORDER);

    UpdateLayout();
}

#endif  // PLATFORM_WIN
