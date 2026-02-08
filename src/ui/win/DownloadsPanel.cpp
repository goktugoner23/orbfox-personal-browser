#ifdef PLATFORM_WIN

#include "DownloadsPanel.h"
#include "DesignSystem.h"
#include "download_manager.h"

#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>

bool DownloadsPanel::class_registered_ = false;

DownloadsPanel::DownloadsPanel() = default;

DownloadsPanel::~DownloadsPanel() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_small_) DeleteObject(font_small_);
    if (font_bold_) DeleteObject(font_bold_);
    if (hwnd_) {
        KillTimer(hwnd_, kTimerId);
        DestroyWindow(hwnd_);
    }
}

bool DownloadsPanel::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = DownloadsPanel::WndProc;
        wcex.cbWndExtra = sizeof(DownloadsPanel*);
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

void DownloadsPanel::Show() {
    if (hwnd_) {
        Refresh();
        ShowWindow(hwnd_, SW_SHOW);
        // Start timer for progress updates
        SetTimer(hwnd_, kTimerId, 500, nullptr);
    }
}

void DownloadsPanel::Hide() {
    if (hwnd_) {
        KillTimer(hwnd_, kTimerId);
        ShowWindow(hwnd_, SW_HIDE);
    }
}

bool DownloadsPanel::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void DownloadsPanel::Refresh() {
    downloads_ = DownloadManager::GetInstance().GetDownloads();

    // Sort: in-progress first, then by end time descending
    std::sort(downloads_.begin(), downloads_.end(), [](const DownloadItem& a, const DownloadItem& b) {
        bool aActive = (a.state == DownloadState::InProgress || a.state == DownloadState::Paused);
        bool bActive = (b.state == DownloadState::InProgress || b.state == DownloadState::Paused);
        if (aActive != bActive) return aActive;
        return a.end_time > b.end_time;
    });

    content_height_ = static_cast<int>(downloads_.size()) * kRowHeight;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK DownloadsPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DownloadsPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<DownloadsPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<DownloadsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) return panel->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT DownloadsPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_TIMER:
            if (wParam == kTimerId) OnTimer();
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void DownloadsPanel::OnCreate() {
    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_small_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeSmall());
    font_bold_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody(), FW_SEMIBOLD);
}

void DownloadsPanel::OnPaint() {
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
    DrawTextW(memDC, L"Downloads", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Download items
    int y = kHeaderHeight - scroll_offset_;
    for (size_t i = 0; i < downloads_.size(); i++) {
        if (y + kRowHeight > kHeaderHeight && y < clientRect.bottom) {
            DrawDownloadRow(memDC, downloads_[i], y, static_cast<int>(i) == hovered_row_);
        }
        y += kRowHeight;
    }

    if (downloads_.empty()) {
        RECT emptyRect = { 0, kHeaderHeight, clientRect.right, kHeaderHeight + 60 };
        SelectObject(memDC, font_normal_);
        SetTextColor(memDC, DesignSystem::GetTextSecondaryColor());
        DrawTextW(memDC, L"No downloads", -1, &emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void DownloadsPanel::DrawDownloadRow(HDC hdc, const DownloadItem& item, int y, bool hovered) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    RECT rowRect = { 8, y, clientRect.right - 8, y + kRowHeight - 4 };

    if (hovered) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Filename
    std::wstring filename = DesignSystem::Utf8ToWide(item.filename);
    if (filename.length() > 30) filename = filename.substr(0, 27) + L"...";

    RECT filenameRect = { 16, y + 6, clientRect.right - 16, y + 24 };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_bold_);
    SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
    DrawTextW(hdc, filename.c_str(), -1, &filenameRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool isActive = (item.state == DownloadState::InProgress || item.state == DownloadState::Paused);

    if (isActive && item.total_bytes > 0) {
        // Progress bar
        DrawProgressBar(hdc, 16, y + 28, clientRect.right - 80, 8, item.percent_complete);

        // Size progress
        std::wstring progress = FormatFileSize(item.received_bytes) + L" / " + FormatFileSize(item.total_bytes);
        RECT progressRect = { clientRect.right - 70, y + 24, clientRect.right - 8, y + 40 };
        SelectObject(hdc, font_small_);
        SetTextColor(hdc, DesignSystem::GetTextSecondaryColor());
        DrawTextW(hdc, progress.c_str(), -1, &progressRect, DT_RIGHT | DT_SINGLELINE);
    } else {
        // Status text
        std::wstring status = GetStateText(item.state);
        if (item.state == DownloadState::Complete && item.total_bytes > 0) {
            status += L" - " + FormatFileSize(item.total_bytes);
        }

        RECT statusRect = { 16, y + 28, clientRect.right - 16, y + 44 };
        SelectObject(hdc, font_small_);
        SetTextColor(hdc, GetStateColor(item.state));
        DrawTextW(hdc, status.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
}

void DownloadsPanel::DrawProgressBar(HDC hdc, int x, int y, int width, int height, int percent) {
    // Background
    RECT bgRect = { x, y, x + width, y + height };
    DesignSystem::DrawRoundedRect(hdc, bgRect, height / 2, DesignSystem::GetSurfaceActiveColor());

    // Progress
    if (percent > 0) {
        int progressWidth = (width * percent) / 100;
        RECT progressRect = { x, y, x + progressWidth, y + height };
        DesignSystem::DrawRoundedRect(hdc, progressRect, height / 2, DesignSystem::GetAccentColor());
    }
}

void DownloadsPanel::OnSize(int width, int height) {
    (void)width;
    (void)height;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DownloadsPanel::OnMouseMove(int x, int y) {
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

void DownloadsPanel::OnLButtonUp(int x, int y) {
    int row = HitTestRow(x, y);
    if (row >= 0 && static_cast<size_t>(row) < downloads_.size()) {
        const DownloadItem& item = downloads_[row];
        if (item.state == DownloadState::Complete) {
            // Open file
            std::wstring path = DesignSystem::Utf8ToWide(item.full_path);
            ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
}

void DownloadsPanel::OnRButtonUp(int x, int y) {
    int row = HitTestRow(x, y);
    if (row >= 0) {
        ShowContextMenu(x, y, row);
    }
}

void DownloadsPanel::OnMouseWheel(int delta) {
    scroll_offset_ -= delta / 3;
    scroll_offset_ = std::max(0, std::min(scroll_offset_, content_height_ - 100));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DownloadsPanel::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_row_ >= 0) {
        hovered_row_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void DownloadsPanel::OnTimer() {
    // Refresh to update progress
    Refresh();
}

int DownloadsPanel::HitTestRow(int x, int y) {
    (void)x;
    if (y < kHeaderHeight) return -1;

    int row = (y - kHeaderHeight + scroll_offset_) / kRowHeight;
    if (row >= 0 && static_cast<size_t>(row) < downloads_.size()) {
        return row;
    }
    return -1;
}

void DownloadsPanel::ShowContextMenu(int x, int y, int index) {
    if (index < 0 || static_cast<size_t>(index) >= downloads_.size()) return;

    const DownloadItem& item = downloads_[index];

    HMENU menu = CreatePopupMenu();

    if (item.state == DownloadState::InProgress) {
        AppendMenuW(menu, MF_STRING, 1, L"Cancel Download");
    } else if (item.state == DownloadState::Complete) {
        AppendMenuW(menu, MF_STRING, 2, L"Open File");
        AppendMenuW(menu, MF_STRING, 3, L"Show in Folder");
    } else if (item.state == DownloadState::Canceled || item.state == DownloadState::Interrupted) {
        AppendMenuW(menu, MF_STRING, 4, L"Retry Download");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 5, L"Remove from List");

    POINT pt = { x, y };
    ClientToScreen(hwnd_, &pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
        case 1:  // Cancel
            DownloadManager::GetInstance().CancelDownload(item.id);
            Refresh();
            break;
        case 2:  // Open file
            {
                std::wstring path = DesignSystem::Utf8ToWide(item.full_path);
                ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        case 3:  // Show in folder
            {
                std::wstring path = DesignSystem::Utf8ToWide(item.full_path);
                std::wstring cmd = L"/select,\"" + path + L"\"";
                ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
            }
            break;
        case 4:  // Retry
            // TODO: Implement retry
            break;
        case 5:  // Remove from list
            DownloadManager::GetInstance().RemoveDownload(item.id);
            Refresh();
            break;
    }
}

std::wstring DownloadsPanel::FormatFileSize(int64_t bytes) {
    if (bytes < 1024) {
        return std::to_wstring(bytes) + L" B";
    } else if (bytes < 1024 * 1024) {
        return std::to_wstring(bytes / 1024) + L" KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return std::to_wstring(bytes / (1024 * 1024)) + L" MB";
    } else {
        wchar_t buf[32];
        swprintf_s(buf, L"%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        return buf;
    }
}

std::wstring DownloadsPanel::GetStateText(DownloadState state) {
    switch (state) {
        case DownloadState::InProgress: return L"Downloading...";
        case DownloadState::Complete: return L"Complete";
        case DownloadState::Canceled: return L"Canceled";
        case DownloadState::Paused: return L"Paused";
        case DownloadState::Interrupted: return L"Failed";
        default: return L"Unknown";
    }
}

COLORREF DownloadsPanel::GetStateColor(DownloadState state) {
    switch (state) {
        case DownloadState::Complete: return DesignSystem::GetSuccessColor();
        case DownloadState::Canceled:
        case DownloadState::Interrupted: return DesignSystem::GetErrorColor();
        default: return DesignSystem::GetTextSecondaryColor();
    }
}

#endif  // PLATFORM_WIN
