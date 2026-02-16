#ifdef PLATFORM_WIN

#include "ToolbarWindow.h"
#include "DesignSystem.h"
#include "settings_storage.h"
#include "tab_manager.h"

#include <windowsx.h>

bool ToolbarWindow::class_registered_ = false;

ToolbarWindow::ToolbarWindow(TabManager* tab_manager)
    : tab_manager_(tab_manager) {
}

ToolbarWindow::~ToolbarWindow() {
    if (font_) DeleteObject(font_);
    if (avatar_bitmap_) DeleteObject(avatar_bitmap_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool ToolbarWindow::RegisterWindowClass(HINSTANCE hInstance) {
    if (class_registered_) return true;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ToolbarWindow::WndProc;
    wcex.cbWndExtra = sizeof(ToolbarWindow*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wcex)) return false;

    class_registered_ = true;
    return true;
}

bool ToolbarWindow::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!RegisterWindowClass(hInstance)) return false;

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        parent,
        nullptr,
        hInstance,
        this
    );

    return hwnd_ != nullptr;
}

LRESULT CALLBACK ToolbarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolbarWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<ToolbarWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<ToolbarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ToolbarWindow::UrlEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolbarWindow* toolbar = reinterpret_cast<ToolbarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        if (toolbar) {
            toolbar->OnUrlSubmit();
        }
        return 0;
    }

    if (msg == WM_CHAR && wParam == VK_RETURN) {
        // Suppress the beep for Enter key
        return 0;
    }

    if (toolbar && toolbar->original_url_edit_proc_) {
        return CallWindowProc(toolbar->original_url_edit_proc_, hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT ToolbarWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
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

        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, DesignSystem::GetTextPrimaryColor());
            SetBkColor(hdcEdit, DesignSystem::GetSurfaceColor());
            static HBRUSH editBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
            return (LRESULT)editBrush;
        }

        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void ToolbarWindow::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    font_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeBody());

    // Create URL edit control
    url_edit_ = CreateWindowExW(
        0,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(ID_URL_EDIT),
        hInstance,
        nullptr
    );

    if (url_edit_) {
        SendMessage(url_edit_, WM_SETFONT, (WPARAM)font_, TRUE);

        // Subclass the edit control to handle Enter key
        SetWindowLongPtr(url_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        original_url_edit_proc_ = (WNDPROC)SetWindowLongPtr(url_edit_, GWLP_WNDPROC,
                                                             reinterpret_cast<LONG_PTR>(UrlEditProc));
    }

    UpdateLayout();
}

void ToolbarWindow::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetBackgroundColor());
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw top border
    HPEN borderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, 0, nullptr);
    LineTo(memDC, clientRect.right, 0);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Draw buttons
    DrawButtons(memDC);

    // Draw security indicator
    DrawSecurityIndicator(memDC);

    // Draw avatar button
    DrawAvatarButton(memDC);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void ToolbarWindow::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == ID_URL_EDIT && code == EN_SETFOCUS) {
        // Select all text when URL bar is focused
        SendMessage(url_edit_, EM_SETSEL, 0, -1);
    }
}

void ToolbarWindow::OnMouseMove(int x, int y) {
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Check button hover (nav buttons)
    int buttonX = kSidebarWidth + DesignSystem::GetSpacingSM();
    int buttonY = (clientRect.bottom - kButtonSize) / 2;

    int newHovered = -1;
    for (int i = 0; i < 3; i++) {
        RECT buttonRect = { buttonX, buttonY, buttonX + kButtonSize, buttonY + kButtonSize };
        POINT pt = { x, y };
        if (PtInRect(&buttonRect, pt)) {
            newHovered = i;
            break;
        }
        buttonX += kButtonSize + kButtonSpacing;
    }

    // Check avatar button hover
    if (newHovered == -1) {
        int avatarX = clientRect.right - kAvatarSize - DesignSystem::GetSpacingSM();
        int avatarY = (clientRect.bottom - kAvatarSize) / 2;
        RECT avatarRect = { avatarX, avatarY, avatarX + kAvatarSize, avatarY + kAvatarSize };
        POINT pt = { x, y };
        if (PtInRect(&avatarRect, pt)) {
            newHovered = 3;  // Avatar button
        }
    }

    if (newHovered != hovered_button_) {
        hovered_button_ = newHovered;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ToolbarWindow::OnLButtonDown(int x, int y) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Check nav buttons
    int buttonX = kSidebarWidth + DesignSystem::GetSpacingSM();
    int buttonY = (clientRect.bottom - kButtonSize) / 2;

    for (int i = 0; i < 3; i++) {
        RECT buttonRect = { buttonX, buttonY, buttonX + kButtonSize, buttonY + kButtonSize };
        POINT pt = { x, y };
        if (PtInRect(&buttonRect, pt)) {
            if (on_navigation_) {
                switch (i) {
                    case 0:
                        if (can_go_back_) on_navigation_(NavigationAction::Back);
                        break;
                    case 1:
                        if (can_go_forward_) on_navigation_(NavigationAction::Forward);
                        break;
                    case 2:
                        on_navigation_(NavigationAction::Reload);
                        break;
                }
            }
            return;
        }
        buttonX += kButtonSize + kButtonSpacing;
    }

    // Check avatar button
    int avatarX = clientRect.right - kAvatarSize - DesignSystem::GetSpacingSM();
    int avatarY = (clientRect.bottom - kAvatarSize) / 2;
    RECT avatarRect = { avatarX, avatarY, avatarX + kAvatarSize, avatarY + kAvatarSize };
    POINT pt = { x, y };
    if (PtInRect(&avatarRect, pt)) {
        if (on_avatar_click_) {
            on_avatar_click_();
        }
        return;
    }
}

void ToolbarWindow::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_button_ >= 0) {
        hovered_button_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ToolbarWindow::DrawButtons(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int x = kSidebarWidth + DesignSystem::GetSpacingSM();
    int y = (clientRect.bottom - kButtonSize) / 2;

    const wchar_t* icons[] = { L"\u25C0", L"\u25B6", L"\u21BB" };  // ◀ ▶ ↻
    bool enabled[] = { can_go_back_, can_go_forward_, true };

    HFONT oldFont = (HFONT)SelectObject(hdc, font_);

    for (int i = 0; i < 3; i++) {
        RECT buttonRect = { x, y, x + kButtonSize, y + kButtonSize };

        // Background
        COLORREF bgColor = DesignSystem::GetBackgroundColor();
        if (i == hovered_button_ && enabled[i]) {
            bgColor = DesignSystem::GetSurfaceHoverColor();
        }

        DesignSystem::DrawRoundedRect(hdc, buttonRect, DesignSystem::GetCornerRadiusSmall(), bgColor);

        // Icon
        COLORREF textColor = enabled[i] ? DesignSystem::GetTextPrimaryColor() : DesignSystem::GetTextTertiaryColor();
        DesignSystem::DrawTextWithColor(hdc, icons[i], buttonRect, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        x += kButtonSize + kButtonSpacing;
    }

    SelectObject(hdc, oldFont);
}

void ToolbarWindow::DrawSecurityIndicator(HDC hdc) {
    // Get current URL
    int len = GetWindowTextLengthW(url_edit_);
    if (len <= 0) return;

    std::wstring url(len + 1, 0);
    GetWindowTextW(url_edit_, &url[0], len + 1);
    url.resize(len);

    // Check if HTTPS
    bool isSecure = (url.find(L"https://") == 0);
    bool isHttp = (url.find(L"http://") == 0);

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Position to the left of URL bar
    int buttonAreaWidth = kSidebarWidth + DesignSystem::GetSpacingSM() + (kButtonSize + kButtonSpacing) * 3;
    int indicatorX = buttonAreaWidth + DesignSystem::GetSpacingSM();
    int indicatorY = (clientRect.bottom - DesignSystem::GetIconSizeSmall()) / 2;
    int iconSize = DesignSystem::GetIconSizeSmall();

    HFONT oldFont = (HFONT)SelectObject(hdc, font_);

    // Draw shield icon (tracking protection) — matches macOS
    if (isSecure || isHttp) {
        RECT shieldRect = { indicatorX, indicatorY,
                            indicatorX + iconSize, indicatorY + iconSize };

        const wchar_t* shieldIcon = (blocked_count_ > 0) ? L"\U0001F6E1" : L"\U0001F6E1";  // 🛡
        COLORREF shieldColor = (blocked_count_ > 0) ? DesignSystem::GetAccentColor() : DesignSystem::GetTextTertiaryColor();
        DesignSystem::DrawTextWithColor(hdc, shieldIcon, shieldRect, shieldColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Draw blocked count badge next to shield
        if (blocked_count_ > 0) {
            std::wstring badge = (blocked_count_ < 100)
                ? std::to_wstring(blocked_count_)
                : L"99+";
            RECT badgeRect = { indicatorX + iconSize - 4, indicatorY - 2,
                               indicatorX + iconSize + 14, indicatorY + 10 };
            HFONT smallFont = CreateFont(10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT prevFont = (HFONT)SelectObject(hdc, smallFont);
            DesignSystem::DrawTextWithColor(hdc, badge, badgeRect, DesignSystem::GetAccentColor(), DT_LEFT | DT_TOP | DT_SINGLELINE);
            SelectObject(hdc, prevFont);
            DeleteObject(smallFont);
        }

        indicatorX += iconSize + DesignSystem::GetSpacingXS();
    }

    // Draw lock/unlock icon
    if (isSecure || isHttp) {
        RECT lockRect = { indicatorX, indicatorY,
                          indicatorX + iconSize, indicatorY + iconSize };
        const wchar_t* lockIcon = isSecure ? L"\U0001F512" : L"\U0001F513";  // 🔒 🔓
        COLORREF lockColor = isSecure ? DesignSystem::GetSuccessColor() : DesignSystem::GetWarningColor();
        DesignSystem::DrawTextWithColor(hdc, lockIcon, lockRect, lockColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
}

void ToolbarWindow::UpdateLayout() {
    if (!hwnd_ || !url_edit_) return;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Calculate URL bar position
    int buttonAreaWidth = kSidebarWidth + DesignSystem::GetSpacingSM() + (kButtonSize + kButtonSpacing) * 3;
    int securityIndicatorWidth = DesignSystem::GetIconSizeSmall() + DesignSystem::GetSpacingSM();

    // Leave room for avatar on the right
    int avatarAreaWidth = kAvatarSize + DesignSystem::GetSpacingSM() * 2;

    int urlX = buttonAreaWidth + securityIndicatorWidth + DesignSystem::GetSpacingSM();
    int urlY = (clientRect.bottom - kUrlBarHeight) / 2;
    int urlWidth = clientRect.right - urlX - avatarAreaWidth;

    SetWindowPos(url_edit_, nullptr,
        urlX, urlY,
        urlWidth, kUrlBarHeight,
        SWP_NOZORDER);
}

void ToolbarWindow::OnUrlSubmit() {
    std::string url = GetUrl();
    if (url.empty()) return;

    url = SettingsStorage::GetInstance().ResolveAddressBarInput(url);

    if (on_url_submit_) {
        on_url_submit_(url);
    }
}

void ToolbarWindow::SetUrl(const std::string& url) {
    if (url_edit_) {
        std::wstring wurl = DesignSystem::Utf8ToWide(url);
        SetWindowTextW(url_edit_, wurl.c_str());
        InvalidateRect(hwnd_, nullptr, FALSE);  // Redraw security indicator
    }
}

std::string ToolbarWindow::GetUrl() const {
    if (!url_edit_) return "";

    int len = GetWindowTextLengthW(url_edit_);
    if (len <= 0) return "";

    std::wstring url(len + 1, 0);
    GetWindowTextW(url_edit_, &url[0], len + 1);
    url.resize(len);

    return DesignSystem::WideToUtf8(url);
}

void ToolbarWindow::SetCanGoBack(bool can) {
    can_go_back_ = can;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::SetCanGoForward(bool can) {
    can_go_forward_ = can;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::SetLoading(bool loading) {
    is_loading_ = loading;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::FocusUrlBar() {
    if (url_edit_) {
        SetFocus(url_edit_);
        SendMessage(url_edit_, EM_SETSEL, 0, -1);  // Select all
    }
}

void ToolbarWindow::SetBlockedCount(int count) {
    blocked_count_ = count;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::DrawAvatarButton(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int x = clientRect.right - kAvatarSize - DesignSystem::GetSpacingSM();
    int y = (clientRect.bottom - kAvatarSize) / 2;

    // Draw circular background
    COLORREF bgColor = DesignSystem::GetSurfaceColor();
    if (hovered_button_ == 3) {
        bgColor = DesignSystem::GetSurfaceHoverColor();
    }

    // Draw circle using GDI+ for smoother rendering
    HBRUSH circleBrush = CreateSolidBrush(bgColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, circleBrush);
    HPEN circlePen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, circlePen);

    Ellipse(hdc, x, y, x + kAvatarSize, y + kAvatarSize);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(circleBrush);
    DeleteObject(circlePen);

    if (avatar_bitmap_ && is_signed_in_) {
        // Draw the avatar image (clipped to circle)
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, avatar_bitmap_);

        // Create circular clip region
        HRGN clipRgn = CreateEllipticRgn(x, y, x + kAvatarSize, y + kAvatarSize);
        SelectClipRgn(hdc, clipRgn);

        // Stretch the bitmap to fit
        BITMAP bm;
        GetObject(avatar_bitmap_, sizeof(bm), &bm);
        StretchBlt(hdc, x, y, kAvatarSize, kAvatarSize,
                   memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

        SelectClipRgn(hdc, nullptr);
        DeleteObject(clipRgn);

        SelectObject(memDC, oldBitmap);
        DeleteDC(memDC);
    } else {
        // Draw default person icon
        RECT iconRect = { x, y, x + kAvatarSize, y + kAvatarSize };
        COLORREF iconColor = is_signed_in_ ? DesignSystem::GetTextPrimaryColor() : DesignSystem::GetTextTertiaryColor();

        // Simple person icon using unicode
        const wchar_t* personIcon = L"\U0001F464";  // 👤
        HFONT iconFont = CreateFont(kAvatarSize - 8, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Emoji");
        HFONT oldFont = (HFONT)SelectObject(hdc, iconFont);
        DesignSystem::DrawTextWithColor(hdc, personIcon, iconRect, iconColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(iconFont);
    }
}

void ToolbarWindow::SetSignedIn(bool signed_in) {
    is_signed_in_ = signed_in;
    if (!signed_in) {
        // Clear avatar
        if (avatar_bitmap_) {
            DeleteObject(avatar_bitmap_);
            avatar_bitmap_ = nullptr;
        }
        avatar_url_.clear();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::SetAvatarImage(HBITMAP bitmap) {
    if (avatar_bitmap_) {
        DeleteObject(avatar_bitmap_);
    }
    avatar_bitmap_ = bitmap;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::SetAvatarUrl(const std::string& url) {
    if (url == avatar_url_ || url.empty()) return;
    avatar_url_ = url;

    // Load image asynchronously using WinHTTP
    // For now, this is a placeholder - full implementation would need async download
    // The MainWindow should call SetAvatarImage with the downloaded bitmap

    // TODO: Implement async image download
    // For now, the MainWindow is responsible for downloading and calling SetAvatarImage
}

#endif  // PLATFORM_WIN
