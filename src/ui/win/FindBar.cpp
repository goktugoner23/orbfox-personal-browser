#ifdef PLATFORM_WIN

#include "FindBar.h"
#include "DesignSystem.h"

#include <windowsx.h>

bool FindBar::class_registered_ = false;

FindBar::FindBar() = default;

FindBar::~FindBar() {
    if (font_) DeleteObject(font_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool FindBar::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = FindBar::WndProc;
        wcex.cbWndExtra = sizeof(FindBar*);
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

void FindBar::Show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        Focus();
    }
}

void FindBar::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        // Clear search
        if (search_edit_) SetWindowTextW(search_edit_, L"");
        match_count_ = 0;
        current_match_ = 0;
    }
}

bool FindBar::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void FindBar::SetMatchCount(int count, int current) {
    match_count_ = count;
    current_match_ = current;

    if (match_label_) {
        wchar_t buf[64];
        if (count > 0) {
            swprintf_s(buf, L"%d of %d", current, count);
        } else if (search_text_.empty()) {
            buf[0] = L'\0';
        } else {
            wcscpy_s(buf, L"No matches");
        }
        SetWindowTextW(match_label_, buf);
    }
}

void FindBar::Focus() {
    if (search_edit_) {
        SetFocus(search_edit_);
        SendMessage(search_edit_, EM_SETSEL, 0, -1);
    }
}

LRESULT CALLBACK FindBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FindBar* bar = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        bar = reinterpret_cast<FindBar*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(bar));
        bar->hwnd_ = hwnd;
    } else {
        bar = reinterpret_cast<FindBar*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (bar) return bar->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FindBar::EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FindBar* bar = reinterpret_cast<FindBar*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            // Enter = find next
            if (bar && bar->on_find_ && !bar->search_text_.empty()) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bar->on_find_(DesignSystem::WideToUtf8(bar->search_text_), !shift, false);
            }
            return 0;
        } else if (wParam == VK_ESCAPE) {
            // Escape = close
            if (bar) {
                bar->Hide();
                if (bar->on_close_) bar->on_close_();
            }
            return 0;
        }
    }

    if (msg == WM_CHAR) {
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            return 0;  // Suppress beep
        }
    }

    if (bar && bar->original_edit_proc_) {
        return CallWindowProc(bar->original_edit_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT FindBar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
            SetBkColor(hdc, DesignSystem::GetSurfaceColor());
            static HBRUSH brush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
            return (LRESULT)brush;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void FindBar::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    font_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeBody());

    // Search input
    search_edit_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_SEARCH, hInstance, nullptr
    );
    SendMessage(search_edit_, WM_SETFONT, (WPARAM)font_, TRUE);

    SetWindowLongPtr(search_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    original_edit_proc_ = (WNDPROC)SetWindowLongPtr(search_edit_, GWLP_WNDPROC,
                                                     reinterpret_cast<LONG_PTR>(EditProc));

    // Previous button
    prev_button_ = CreateWindowExW(
        0, L"BUTTON", L"\u25C0",  // ◀
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_PREV, hInstance, nullptr
    );
    SendMessage(prev_button_, WM_SETFONT, (WPARAM)font_, TRUE);

    // Next button
    next_button_ = CreateWindowExW(
        0, L"BUTTON", L"\u25B6",  // ▶
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_NEXT, hInstance, nullptr
    );
    SendMessage(next_button_, WM_SETFONT, (WPARAM)font_, TRUE);

    // Match count label
    match_label_ = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessage(match_label_, WM_SETFONT, (WPARAM)font_, TRUE);

    // Close button
    close_button_ = CreateWindowExW(
        0, L"BUTTON", L"\u2715",  // ✕
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        hwnd_, (HMENU)ID_CLOSE, hInstance, nullptr
    );
    SendMessage(close_button_, WM_SETFONT, (WPARAM)font_, TRUE);

    UpdateLayout();
}

void FindBar::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    HDC hdc = GetDC(hwnd_);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Bottom border
    HPEN borderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, clientRect.bottom - 1, nullptr);
    LineTo(hdc, clientRect.right, clientRect.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    ReleaseDC(hwnd_, hdc);
}

void FindBar::OnSize(int width, int height) {
    (void)width;
    (void)height;
    UpdateLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FindBar::UpdateLayout() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    int y = (height - 24) / 2;
    int x = 8;

    // Search input (expandable)
    int searchWidth = width - 200;
    SetWindowPos(search_edit_, nullptr, x, y, searchWidth, 24, SWP_NOZORDER);
    x += searchWidth + 8;

    // Previous/Next buttons
    SetWindowPos(prev_button_, nullptr, x, y, 28, 24, SWP_NOZORDER);
    x += 32;
    SetWindowPos(next_button_, nullptr, x, y, 28, 24, SWP_NOZORDER);
    x += 36;

    // Match label
    SetWindowPos(match_label_, nullptr, x, y, 70, 24, SWP_NOZORDER);
    x += 78;

    // Close button
    SetWindowPos(close_button_, nullptr, width - 36, y, 28, 24, SWP_NOZORDER);
}

void FindBar::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    switch (id) {
        case ID_SEARCH:
            if (code == EN_CHANGE) {
                OnFindTextChanged();
            }
            break;
        case ID_PREV:
            if (on_find_ && !search_text_.empty()) {
                on_find_(DesignSystem::WideToUtf8(search_text_), false, false);
            }
            break;
        case ID_NEXT:
            if (on_find_ && !search_text_.empty()) {
                on_find_(DesignSystem::WideToUtf8(search_text_), true, false);
            }
            break;
        case ID_CLOSE:
            Hide();
            if (on_close_) on_close_();
            break;
    }
}

void FindBar::OnFindTextChanged() {
    int len = GetWindowTextLengthW(search_edit_);
    search_text_.resize(len + 1);
    GetWindowTextW(search_edit_, &search_text_[0], len + 1);
    search_text_.resize(len);

    // Trigger search
    if (on_find_) {
        on_find_(DesignSystem::WideToUtf8(search_text_), true, false);
    }
}

#endif  // PLATFORM_WIN
