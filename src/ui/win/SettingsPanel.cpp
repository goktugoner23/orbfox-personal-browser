#ifdef PLATFORM_WIN

#include "SettingsPanel.h"
#include "DesignSystem.h"
#include "settings_storage.h"
#include "history_storage.h"

#include <windowsx.h>
#include <shlobj.h>
#include <algorithm>

bool SettingsPanel::class_registered_ = false;

// Forward declaration for clearing history
extern HistoryStorage* GetHistoryStorage();

// Application version - should match the main app
static const wchar_t* kAppVersion = L"1.0.0";

SettingsPanel::SettingsPanel() = default;

SettingsPanel::~SettingsPanel() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_bold_) DeleteObject(font_bold_);
    if (font_small_) DeleteObject(font_small_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool SettingsPanel::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = SettingsPanel::WndProc;
        wcex.cbWndExtra = sizeof(SettingsPanel*);
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = kClassName;

        if (!RegisterClassExW(&wcex)) return false;
        class_registered_ = true;
    }

    hwnd_ = CreateWindowExW(
        0, kClassName, nullptr,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        parent, nullptr, hInstance, this
    );

    return hwnd_ != nullptr;
}

void SettingsPanel::Show() {
    if (hwnd_) {
        LoadSettings();
        ShowWindow(hwnd_, SW_SHOW);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SettingsPanel::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

bool SettingsPanel::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void SettingsPanel::Refresh() {
    LoadSettings();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsPanel::LoadSettings() {
    Settings settings = SettingsStorage::GetInstance().Get();

    restore_session_ = settings.restore_session;
    tracking_protection_ = settings.tracking_protection;
    ask_before_download_ = settings.ask_before_download;

    // Update edit controls
    if (new_tab_url_edit_) {
        std::wstring url = DesignSystem::Utf8ToWide(settings.new_tab_url);
        SetWindowTextW(new_tab_url_edit_, url.c_str());
    }

    if (download_path_edit_) {
        std::wstring path = DesignSystem::Utf8ToWide(
            settings.download_path.empty() ? "~/Downloads" : settings.download_path
        );
        SetWindowTextW(download_path_edit_, path.c_str());
    }
}

void SettingsPanel::SaveSettings() {
    Settings settings = SettingsStorage::GetInstance().Get();

    settings.restore_session = restore_session_;
    settings.tracking_protection = tracking_protection_;
    settings.ask_before_download = ask_before_download_;

    // Get new tab URL from edit control
    if (new_tab_url_edit_) {
        int len = GetWindowTextLengthW(new_tab_url_edit_);
        std::wstring url(len + 1, L'\0');
        GetWindowTextW(new_tab_url_edit_, &url[0], len + 1);
        url.resize(len);
        settings.new_tab_url = DesignSystem::WideToUtf8(url);
    }

    // Get download path from edit control
    if (download_path_edit_) {
        int len = GetWindowTextLengthW(download_path_edit_);
        std::wstring path(len + 1, L'\0');
        GetWindowTextW(download_path_edit_, &path[0], len + 1);
        path.resize(len);
        std::string pathUtf8 = DesignSystem::WideToUtf8(path);
        // Only set if not the default display value
        if (pathUtf8 != "~/Downloads") {
            settings.download_path = pathUtf8;
        } else {
            settings.download_path = "";  // Empty means default
        }
    }

    SettingsStorage::GetInstance().Set(settings);
}

LRESULT CALLBACK SettingsPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<SettingsPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<SettingsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) return panel->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsPanel::EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsPanel* panel = reinterpret_cast<SettingsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // Save settings when user presses Enter or edit loses focus
    if (msg == WM_KILLFOCUS || (msg == WM_KEYDOWN && wParam == VK_RETURN)) {
        if (panel) {
            panel->SaveSettings();
        }
    }

    if (panel && panel->original_edit_proc_) {
        return CallWindowProc(panel->original_edit_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT SettingsPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;
        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, DesignSystem::GetTextPrimaryColor());
            SetBkColor(hdcEdit, DesignSystem::GetBackgroundColor());
            static HBRUSH editBrush = CreateSolidBrush(DesignSystem::GetBackgroundColor());
            return (LRESULT)editBrush;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void SettingsPanel::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    font_normal_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeBody());
    font_bold_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeBody(), FW_SEMIBOLD);
    font_small_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeSmall());

    // New Tab URL edit
    new_tab_url_edit_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_NEW_TAB_URL_EDIT, hInstance, nullptr
    );
    SendMessage(new_tab_url_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    // Subclass the edit control for save-on-blur
    SetWindowLongPtr(new_tab_url_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    original_edit_proc_ = (WNDPROC)SetWindowLongPtr(new_tab_url_edit_, GWLP_WNDPROC,
                                                     reinterpret_cast<LONG_PTR>(EditProc));

    // Download path edit
    download_path_edit_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_DOWNLOAD_PATH_EDIT, hInstance, nullptr
    );
    SendMessage(download_path_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    // Subclass download path edit too
    SetWindowLongPtr(download_path_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtr(download_path_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));

    LoadSettings();
    UpdateLayout();
}

void SettingsPanel::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Header
    RECT headerRect = { 0, 0, clientRect.right, kHeaderHeight };
    HFONT oldFont = (HFONT)SelectObject(memDC, font_bold_);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, DesignSystem::GetTextPrimaryColor());
    DrawTextW(memDC, L"Settings", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Calculate content area
    int y = kHeaderHeight - scroll_offset_;
    int width = clientRect.right;

    // ============================================================
    // GENERAL Section
    // ============================================================
    DrawSectionHeader(memDC, y, L"General");

    // New Tab URL label (edit control is positioned separately)
    DrawInputRow(memDC, y, L"New Tab URL");

    // Restore Session toggle
    DrawToggleRow(memDC, y, L"Restore session on startup", restore_session_, ID_RESTORE_SESSION_TOGGLE);

    // ============================================================
    // PRIVACY Section
    // ============================================================
    DrawSectionHeader(memDC, y, L"Privacy");

    // Tracking protection toggle
    DrawToggleRow(memDC, y, L"Tracking protection", tracking_protection_, ID_TRACKING_PROTECTION_TOGGLE);

    // Clear browsing data button
    DrawButtonRow(memDC, y, L"Clear browsing data", L"Clear", ID_CLEAR_DATA_BUTTON);

    // ============================================================
    // DOWNLOADS Section
    // ============================================================
    DrawSectionHeader(memDC, y, L"Downloads");

    // Download location (edit control positioned separately)
    DrawInputRow(memDC, y, L"Download location");

    // Ask before download toggle
    DrawToggleRow(memDC, y, L"Always ask where to save", ask_before_download_, ID_ASK_DOWNLOAD_TOGGLE);

    // Browse folder button
    DrawButtonRow(memDC, y, L"", L"Browse...", ID_BROWSE_FOLDER_BUTTON);

    // ============================================================
    // ABOUT Section
    // ============================================================
    DrawSectionHeader(memDC, y, L"About");

    // Version info
    {
        RECT rowRect = { kPadding, y, width - kPadding, y + kRowHeight };
        SelectObject(memDC, font_normal_);
        SetTextColor(memDC, DesignSystem::GetTextSecondaryColor());

        std::wstring versionText = L"OrbFox Browser v";
        versionText += kAppVersion;
        DrawTextW(memDC, versionText.c_str(), -1, &rowRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += kRowHeight;
    }

    // Help link
    DrawLinkRow(memDC, y, L"Help & Documentation", ID_HELP_LINK);

    // Feedback link
    DrawLinkRow(memDC, y, L"Send Feedback", ID_FEEDBACK_LINK);

    // Store content height for scrolling
    content_height_ = y + scroll_offset_;

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void SettingsPanel::DrawSectionHeader(HDC hdc, int& y, const std::wstring& title) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    y += kPadding / 2;  // Add some spacing before section

    RECT headerRect = { kPadding, y, clientRect.right - kPadding, y + kSectionHeaderHeight };

    HFONT oldFont = (HFONT)SelectObject(hdc, font_bold_);
    SetTextColor(hdc, DesignSystem::GetAccentColor());
    DrawTextW(hdc, title.c_str(), -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Draw separator line below
    HPEN pen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderSubtleColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, kPadding, y + kSectionHeaderHeight - 4, nullptr);
    LineTo(hdc, clientRect.right - kPadding, y + kSectionHeaderHeight - 4);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    y += kSectionHeaderHeight;
}

void SettingsPanel::DrawToggleRow(HDC hdc, int& y, const std::wstring& label, bool value, int id) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    RECT rowRect = { kPadding, y, width - kPadding, y + kRowHeight };

    // Hover highlight
    if (hovered_item_id_ == id) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Label
    RECT labelRect = { kPadding + 8, y, width - kPadding - kToggleWidth - 16, y + kRowHeight };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
    DrawTextW(hdc, label.c_str(), -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Toggle switch
    int toggleX = width - kPadding - kToggleWidth - 8;
    int toggleY = y + (kRowHeight - kToggleHeight) / 2;

    RECT toggleRect = { toggleX, toggleY, toggleX + kToggleWidth, toggleY + kToggleHeight };

    // Toggle background
    COLORREF toggleBg = value ? DesignSystem::GetAccentColor() : DesignSystem::GetSurfaceActiveColor();
    DesignSystem::DrawRoundedRect(hdc, toggleRect, kToggleHeight / 2, toggleBg);

    // Toggle knob
    int knobSize = kToggleHeight - 4;
    int knobX = value ? (toggleX + kToggleWidth - knobSize - 2) : (toggleX + 2);
    int knobY = toggleY + 2;
    RECT knobRect = { knobX, knobY, knobX + knobSize, knobY + knobSize };
    DesignSystem::DrawRoundedRect(hdc, knobRect, knobSize / 2, RGB(255, 255, 255));

    y += kRowHeight;
}

void SettingsPanel::DrawButtonRow(HDC hdc, int& y, const std::wstring& label, const std::wstring& buttonText, int id) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    RECT rowRect = { kPadding, y, width - kPadding, y + kRowHeight };

    // Hover highlight for entire row
    if (hovered_item_id_ == id) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Label (if provided)
    if (!label.empty()) {
        RECT labelRect = { kPadding + 8, y, width - kPadding - 100, y + kRowHeight };
        HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
        SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
        DrawTextW(hdc, label.c_str(), -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
    }

    // Button
    int buttonWidth = 80;
    int buttonHeight = 28;
    int buttonX = width - kPadding - buttonWidth - 8;
    int buttonY = y + (kRowHeight - buttonHeight) / 2;

    RECT buttonRect = { buttonX, buttonY, buttonX + buttonWidth, buttonY + buttonHeight };

    // Button background
    COLORREF btnBg = (hovered_item_id_ == id) ? DesignSystem::GetAccentHoverColor() : DesignSystem::GetAccentColor();
    DesignSystem::DrawRoundedRect(hdc, buttonRect, 4, btnBg);

    // Button text
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, buttonText.c_str(), -1, &buttonRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    y += kRowHeight;
}

void SettingsPanel::DrawLinkRow(HDC hdc, int& y, const std::wstring& text, int id) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    RECT rowRect = { kPadding, y, width - kPadding, y + kRowHeight };

    // Hover highlight
    if (hovered_item_id_ == id) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Link text
    RECT textRect = { kPadding + 8, y, width - kPadding - 8, y + kRowHeight };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    SetTextColor(hdc, DesignSystem::GetAccentColor());
    DrawTextW(hdc, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    y += kRowHeight;
}

void SettingsPanel::DrawInputRow(HDC hdc, int& y, const std::wstring& label) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    // Label
    RECT labelRect = { kPadding + 8, y, width - kPadding, y + 24 };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_small_);
    SetTextColor(hdc, DesignSystem::GetTextSecondaryColor());
    DrawTextW(hdc, label.c_str(), -1, &labelRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Edit control is positioned in UpdateLayout()
    y += kInputRowHeight;
}

void SettingsPanel::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsPanel::UpdateLayout() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    // Calculate Y positions for edit controls
    // These must match the drawing order in OnPaint

    int y = kHeaderHeight - scroll_offset_;

    // General section header
    y += kPadding / 2 + kSectionHeaderHeight;

    // New Tab URL input - position edit control
    int editY = y + 20;  // Offset for label
    SetWindowPos(new_tab_url_edit_, nullptr,
        kPadding + 8, editY,
        width - kPadding * 2 - 16, 28,
        SWP_NOZORDER);
    y += kInputRowHeight;

    // Skip restore session toggle
    y += kRowHeight;

    // Privacy section header
    y += kPadding / 2 + kSectionHeaderHeight;

    // Skip tracking protection toggle
    y += kRowHeight;

    // Skip clear data button
    y += kRowHeight;

    // Downloads section header
    y += kPadding / 2 + kSectionHeaderHeight;

    // Download path input - position edit control
    editY = y + 20;
    SetWindowPos(download_path_edit_, nullptr,
        kPadding + 8, editY,
        width - kPadding * 2 - 100, 28,  // Leave room for Browse button
        SWP_NOZORDER);
}

void SettingsPanel::OnMouseMove(int x, int y) {
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    HitTestResult hit = HitTest(x, y);
    int newHovered = (hit.type != 0) ? hit.id : -1;

    if (newHovered != hovered_item_id_) {
        hovered_item_id_ = newHovered;

        // Update cursor
        if (hit.type == 3) {  // Link
            SetCursor(LoadCursor(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }

        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SettingsPanel::OnLButtonUp(int x, int y) {
    HitTestResult hit = HitTest(x, y);

    switch (hit.id) {
        case ID_RESTORE_SESSION_TOGGLE:
            restore_session_ = !restore_session_;
            SaveSettings();
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case ID_TRACKING_PROTECTION_TOGGLE:
            tracking_protection_ = !tracking_protection_;
            SaveSettings();
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case ID_ASK_DOWNLOAD_TOGGLE:
            ask_before_download_ = !ask_before_download_;
            SaveSettings();
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case ID_CLEAR_DATA_BUTTON:
            if (MessageBoxW(hwnd_, L"Clear all browsing history and cookies?",
                           L"Clear Browsing Data", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                // Clear history
                if (HistoryStorage* storage = GetHistoryStorage()) {
                    storage->ClearHistoryBefore(std::time(nullptr));
                }
                // Call external callback for additional cleanup
                if (on_clear_data_) {
                    on_clear_data_();
                }
                MessageBoxW(hwnd_, L"Browsing data cleared.", L"Clear Browsing Data", MB_OK | MB_ICONINFORMATION);
            }
            break;

        case ID_BROWSE_FOLDER_BUTTON: {
            // Use Windows folder browser dialog
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd_;
            bi.lpszTitle = L"Select Download Folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, path)) {
                    SetWindowTextW(download_path_edit_, path);
                    SaveSettings();
                }
                CoTaskMemFree(pidl);
            }
            break;
        }

        case ID_HELP_LINK:
            if (on_open_url_) {
                on_open_url_("https://github.com/nicholasb4711/OrbFox/wiki", false);
            }
            break;

        case ID_FEEDBACK_LINK:
            if (on_open_url_) {
                on_open_url_("https://github.com/nicholasb4711/OrbFox/issues", false);
            }
            break;
    }
}

void SettingsPanel::OnMouseWheel(int delta) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    scroll_offset_ -= delta / 3;
    scroll_offset_ = (std::max)(0, (std::min)(scroll_offset_, static_cast<int>(content_height_ - clientRect.bottom + 50)));

    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsPanel::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_item_id_ >= 0) {
        hovered_item_id_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SettingsPanel::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    // Handle edit control notifications
    if (code == EN_KILLFOCUS) {
        if (id == ID_NEW_TAB_URL_EDIT || id == ID_DOWNLOAD_PATH_EDIT) {
            SaveSettings();
        }
    }
}

SettingsPanel::HitTestResult SettingsPanel::HitTest(int x, int y) {
    HitTestResult result = { 0, -1 };

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    // Reconstruct layout to find hit areas
    int rowY = kHeaderHeight - scroll_offset_;

    // General section
    rowY += kPadding / 2 + kSectionHeaderHeight;
    rowY += kInputRowHeight;  // New tab URL input

    // Restore session toggle
    RECT toggleRect = { kPadding, rowY, width - kPadding, rowY + kRowHeight };
    POINT pt = { x, y };
    if (PtInRect(&toggleRect, pt)) {
        result.type = 1;
        result.id = ID_RESTORE_SESSION_TOGGLE;
        return result;
    }
    rowY += kRowHeight;

    // Privacy section
    rowY += kPadding / 2 + kSectionHeaderHeight;

    // Tracking protection toggle
    toggleRect = { kPadding, rowY, width - kPadding, rowY + kRowHeight };
    if (PtInRect(&toggleRect, pt)) {
        result.type = 1;
        result.id = ID_TRACKING_PROTECTION_TOGGLE;
        return result;
    }
    rowY += kRowHeight;

    // Clear data button
    int buttonWidth = 80;
    int buttonX = width - kPadding - buttonWidth - 8;
    int buttonY = rowY + (kRowHeight - 28) / 2;
    RECT buttonRect = { buttonX, buttonY, buttonX + buttonWidth, buttonY + 28 };
    if (PtInRect(&buttonRect, pt)) {
        result.type = 2;
        result.id = ID_CLEAR_DATA_BUTTON;
        return result;
    }
    rowY += kRowHeight;

    // Downloads section
    rowY += kPadding / 2 + kSectionHeaderHeight;
    rowY += kInputRowHeight;  // Download path input

    // Ask before download toggle
    toggleRect = { kPadding, rowY, width - kPadding, rowY + kRowHeight };
    if (PtInRect(&toggleRect, pt)) {
        result.type = 1;
        result.id = ID_ASK_DOWNLOAD_TOGGLE;
        return result;
    }
    rowY += kRowHeight;

    // Browse folder button
    buttonY = rowY + (kRowHeight - 28) / 2;
    buttonRect = { buttonX, buttonY, buttonX + buttonWidth, buttonY + 28 };
    if (PtInRect(&buttonRect, pt)) {
        result.type = 2;
        result.id = ID_BROWSE_FOLDER_BUTTON;
        return result;
    }
    rowY += kRowHeight;

    // About section
    rowY += kPadding / 2 + kSectionHeaderHeight;
    rowY += kRowHeight;  // Version info

    // Help link
    RECT linkRect = { kPadding, rowY, width - kPadding, rowY + kRowHeight };
    if (PtInRect(&linkRect, pt)) {
        result.type = 3;
        result.id = ID_HELP_LINK;
        return result;
    }
    rowY += kRowHeight;

    // Feedback link
    linkRect = { kPadding, rowY, width - kPadding, rowY + kRowHeight };
    if (PtInRect(&linkRect, pt)) {
        result.type = 3;
        result.id = ID_FEEDBACK_LINK;
        return result;
    }

    return result;
}

#endif  // PLATFORM_WIN
