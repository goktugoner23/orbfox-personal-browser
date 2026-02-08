#ifdef PLATFORM_WIN

#include "HistoryPanel.h"
#include "DesignSystem.h"
#include "history_storage.h"

#include <windowsx.h>
#include <algorithm>
#include <ctime>

bool HistoryPanel::class_registered_ = false;

extern HistoryStorage* GetHistoryStorage();

HistoryPanel::HistoryPanel() = default;

HistoryPanel::~HistoryPanel() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_small_) DeleteObject(font_small_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool HistoryPanel::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = HistoryPanel::WndProc;
        wcex.cbWndExtra = sizeof(HistoryPanel*);
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

void HistoryPanel::Show() {
    if (hwnd_) {
        Refresh();
        ShowWindow(hwnd_, SW_SHOW);
    }
}

void HistoryPanel::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

bool HistoryPanel::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void HistoryPanel::Refresh() {
    HistoryStorage* storage = GetHistoryStorage();
    if (!storage) return;

    entries_ = storage->GetRecentHistory(500);  // Get up to 500 entries
    OnSearchChanged();  // Apply filter
}

LRESULT CALLBACK HistoryPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HistoryPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<HistoryPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<HistoryPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) return panel->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK HistoryPanel::SearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HistoryPanel* panel = reinterpret_cast<HistoryPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_CHAR || msg == WM_KEYDOWN) {
        // Post a message to update filter after the text changes
        if (panel) {
            PostMessage(panel->hwnd_, WM_USER + 1, 0, 0);
        }
    }

    if (panel && panel->original_search_proc_) {
        return CallWindowProc(panel->original_search_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT HistoryPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_RBUTTONUP:
            OnRButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_CLEAR && HIWORD(wParam) == BN_CLICKED) {
                // Clear all history
                if (HistoryStorage* storage = GetHistoryStorage()) {
                    if (MessageBoxW(hwnd_, L"Clear all browsing history?", L"Clear History",
                                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        storage->ClearHistory();
                        Refresh();
                    }
                }
            }
            return 0;
        case WM_USER + 1:  // Search text changed
            OnSearchChanged();
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

void HistoryPanel::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_small_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeSmall());

    // Search box
    search_edit_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_SEARCH, hInstance, nullptr
    );
    SendMessage(search_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
    SendMessage(search_edit_, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search history...");

    SetWindowLongPtr(search_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    original_search_proc_ = (WNDPROC)SetWindowLongPtr(search_edit_, GWLP_WNDPROC,
                                                       reinterpret_cast<LONG_PTR>(SearchEditProc));

    // Clear button
    clear_button_ = CreateWindowExW(
        0, L"BUTTON", L"Clear All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_CLEAR, hInstance, nullptr
    );
    SendMessage(clear_button_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    UpdateLayout();
}

void HistoryPanel::OnPaint() {
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
    RECT headerRect = { 0, 0, clientRect.right, 36 };
    HFONT oldFont = (HFONT)SelectObject(memDC, font_normal_);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, DesignSystem::GetTextPrimaryColor());
    DrawTextW(memDC, L"History", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // History entries
    int y = kHeaderHeight - scroll_offset_;
    for (size_t i = 0; i < filtered_entries_.size(); i++) {
        if (y + kRowHeight > kHeaderHeight && y < clientRect.bottom) {
            DrawHistoryRow(memDC, filtered_entries_[i], y, static_cast<int>(i) == hovered_row_);
        }
        y += kRowHeight;
    }

    if (filtered_entries_.empty()) {
        RECT emptyRect = { 0, kHeaderHeight, clientRect.right, kHeaderHeight + 60 };
        SetTextColor(memDC, DesignSystem::GetTextSecondaryColor());
        DrawTextW(memDC, L"No history", -1, &emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void HistoryPanel::DrawHistoryRow(HDC hdc, const HistoryEntry& entry, int y, bool hovered) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    RECT rowRect = { 8, y, clientRect.right - 8, y + kRowHeight };

    if (hovered) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Title
    std::wstring title = DesignSystem::Utf8ToWide(entry.title.empty() ? entry.url : entry.title);
    if (title.length() > 35) title = title.substr(0, 32) + L"...";

    RECT titleRect = { 16, y + 4, clientRect.right - 16, y + 22 };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
    DrawTextW(hdc, title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    // URL (truncated)
    std::wstring url = DesignSystem::Utf8ToWide(entry.url);
    if (url.length() > 40) url = url.substr(0, 37) + L"...";

    RECT urlRect = { 16, y + 22, clientRect.right - 16, y + 38 };
    SelectObject(hdc, font_small_);
    SetTextColor(hdc, DesignSystem::GetTextSecondaryColor());
    DrawTextW(hdc, url.c_str(), -1, &urlRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldFont);
}

void HistoryPanel::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void HistoryPanel::UpdateLayout() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;

    // Search box
    SetWindowPos(search_edit_, nullptr, 8, 40, width - 16, kSearchHeight, SWP_NOZORDER);

    // Clear button
    SetWindowPos(clear_button_, nullptr, width - 80, 8, 72, 24, SWP_NOZORDER);
}

void HistoryPanel::OnMouseMove(int x, int y) {
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    int row = HitTestRow(x, y);
    if (row != hovered_row_) {
        hovered_row_ = row;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void HistoryPanel::OnLButtonUp(int x, int y) {
    int row = HitTestRow(x, y);
    if (row >= 0 && static_cast<size_t>(row) < filtered_entries_.size()) {
        if (on_open_url_) {
            on_open_url_(filtered_entries_[row].url, false);
        }
    }
}

void HistoryPanel::OnRButtonUp(int x, int y) {
    int row = HitTestRow(x, y);
    if (row >= 0) {
        ShowContextMenu(x, y, row);
    }
}

void HistoryPanel::OnMouseWheel(int delta) {
    content_height_ = static_cast<int>(filtered_entries_.size()) * kRowHeight;
    scroll_offset_ -= delta / 3;
    scroll_offset_ = std::max(0, std::min(scroll_offset_, content_height_ - 100));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void HistoryPanel::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_row_ >= 0) {
        hovered_row_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void HistoryPanel::OnSearchChanged() {
    // Get search text
    int len = GetWindowTextLengthW(search_edit_);
    search_text_.resize(len + 1);
    GetWindowTextW(search_edit_, &search_text_[0], len + 1);
    search_text_.resize(len);

    // Convert to lowercase for case-insensitive search
    std::wstring searchLower = search_text_;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::towlower);

    // Filter entries
    filtered_entries_.clear();
    for (const auto& entry : entries_) {
        if (searchLower.empty()) {
            filtered_entries_.push_back(entry);
        } else {
            std::wstring title = DesignSystem::Utf8ToWide(entry.title);
            std::wstring url = DesignSystem::Utf8ToWide(entry.url);
            std::transform(title.begin(), title.end(), title.begin(), ::towlower);
            std::transform(url.begin(), url.end(), url.begin(), ::towlower);

            if (title.find(searchLower) != std::wstring::npos ||
                url.find(searchLower) != std::wstring::npos) {
                filtered_entries_.push_back(entry);
            }
        }
    }

    scroll_offset_ = 0;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int HistoryPanel::HitTestRow(int x, int y) {
    (void)x;
    if (y < kHeaderHeight) return -1;

    int row = (y - kHeaderHeight + scroll_offset_) / kRowHeight;
    if (row >= 0 && static_cast<size_t>(row) < filtered_entries_.size()) {
        return row;
    }
    return -1;
}

void HistoryPanel::ShowContextMenu(int x, int y, int index) {
    if (index < 0 || static_cast<size_t>(index) >= filtered_entries_.size()) return;

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Open in New Tab");
    AppendMenuW(menu, MF_STRING, 2, L"Open in Background");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Copy URL");
    AppendMenuW(menu, MF_STRING, 4, L"Delete from History");

    POINT pt = { x, y };
    ClientToScreen(hwnd_, &pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    const HistoryEntry& entry = filtered_entries_[index];

    switch (cmd) {
        case 1:
            if (on_open_url_) on_open_url_(entry.url, false);
            break;
        case 2:
            if (on_open_url_) on_open_url_(entry.url, true);
            break;
        case 3: {
            // Copy URL to clipboard
            std::wstring url = DesignSystem::Utf8ToWide(entry.url);
            if (OpenClipboard(hwnd_)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (url.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                    wcscpy_s(pMem, url.size() + 1, url.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            break;
        }
        case 4:
            if (HistoryStorage* storage = GetHistoryStorage()) {
                storage->DeleteEntry(entry.id);
                Refresh();
            }
            break;
    }
}

#endif  // PLATFORM_WIN
