#ifdef PLATFORM_WIN

#include "BookmarksPanel.h"
#include "DesignSystem.h"
#include "bookmark_storage.h"

#include <windowsx.h>
#include <algorithm>

bool BookmarksPanel::class_registered_ = false;

// External function to get bookmark storage
extern BookmarkStorage* GetBookmarkStorage();

BookmarksPanel::BookmarksPanel() = default;

BookmarksPanel::~BookmarksPanel() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_bold_) DeleteObject(font_bold_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool BookmarksPanel::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = BookmarksPanel::WndProc;
        wcex.cbWndExtra = sizeof(BookmarksPanel*);
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

void BookmarksPanel::Show() {
    if (hwnd_) {
        Refresh();
        ShowWindow(hwnd_, SW_SHOW);
    }
}

void BookmarksPanel::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

bool BookmarksPanel::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void BookmarksPanel::Refresh() {
    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) return;

    folders_ = storage->GetFolders();
    bookmarks_ = storage->GetAllBookmarks();

    // Calculate content height
    content_height_ = 0;
    for (const auto& folder : folders_) {
        content_height_ += kRowHeight;  // Folder row
        if (std::find(expanded_folders_.begin(), expanded_folders_.end(), folder) != expanded_folders_.end()) {
            // Count bookmarks in this folder
            for (const auto& bm : bookmarks_) {
                if (bm.folder == folder) {
                    content_height_ += kRowHeight;
                }
            }
        }
    }
    // Root bookmarks (no folder)
    for (const auto& bm : bookmarks_) {
        if (bm.folder.empty()) {
            content_height_ += kRowHeight;
        }
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK BookmarksPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BookmarksPanel* panel = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        panel = reinterpret_cast<BookmarksPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->hwnd_ = hwnd;
    } else {
        panel = reinterpret_cast<BookmarksPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (panel) return panel->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT BookmarksPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void BookmarksPanel::OnCreate() {
    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_bold_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody(), FW_SEMIBOLD);
}

void BookmarksPanel::OnPaint() {
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
    RECT headerRect = { 0, 0, clientRect.right, 40 };
    HFONT oldFont = (HFONT)SelectObject(memDC, font_bold_);
    DesignSystem::DrawTextWithColor(memDC, L"Bookmarks", headerRect,
        DesignSystem::GetTextPrimaryColor(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int y = 40 - scroll_offset_;

    // Draw folders and their bookmarks
    for (size_t fi = 0; fi < folders_.size(); fi++) {
        const auto& folder = folders_[fi];
        bool expanded = std::find(expanded_folders_.begin(), expanded_folders_.end(), folder)
                       != expanded_folders_.end();
        bool folderHovered = (hovered_.type == HitResult::Folder && hovered_.folder_name == folder);

        if (y + kRowHeight > 40 && y < clientRect.bottom) {
            DrawFolderRow(memDC, folder, y, expanded, folderHovered);
        }
        y += kRowHeight;

        if (expanded) {
            for (size_t bi = 0; bi < bookmarks_.size(); bi++) {
                const auto& bm = bookmarks_[bi];
                if (bm.folder == folder) {
                    bool bmHovered = (hovered_.type == HitResult::Bookmark &&
                                      hovered_.index == static_cast<int>(bi));
                    if (y + kRowHeight > 40 && y < clientRect.bottom) {
                        DrawBookmarkRow(memDC, bm, y, bmHovered);
                    }
                    y += kRowHeight;
                }
            }
        }
    }

    // Root bookmarks (no folder)
    for (size_t bi = 0; bi < bookmarks_.size(); bi++) {
        const auto& bm = bookmarks_[bi];
        if (bm.folder.empty()) {
            bool bmHovered = (hovered_.type == HitResult::Bookmark &&
                              hovered_.index == static_cast<int>(bi));
            if (y + kRowHeight > 40 && y < clientRect.bottom) {
                DrawBookmarkRow(memDC, bm, y, bmHovered);
            }
            y += kRowHeight;
        }
    }

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void BookmarksPanel::DrawFolderRow(HDC hdc, const std::string& folder, int y, bool expanded, bool hovered) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    RECT rowRect = { 8, y, clientRect.right - 8, y + kRowHeight };

    if (hovered) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Expand/collapse arrow
    const wchar_t* arrow = expanded ? L"\u25BC" : L"\u25B6";  // ▼ ▶
    RECT arrowRect = { 12, y, 28, y + kRowHeight };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    DesignSystem::DrawTextWithColor(hdc, arrow, arrowRect,
        DesignSystem::GetTextSecondaryColor(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Folder icon and name
    std::wstring name = L"\U0001F4C1 " + DesignSystem::Utf8ToWide(folder);  // 📁
    RECT textRect = { 32, y, clientRect.right - 12, y + kRowHeight };
    SelectObject(hdc, font_bold_);
    DesignSystem::DrawTextWithColor(hdc, name, textRect,
        DesignSystem::GetTextPrimaryColor(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldFont);
}

void BookmarksPanel::DrawBookmarkRow(HDC hdc, const Bookmark& bookmark, int y, bool hovered) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int indent = bookmark.folder.empty() ? 8 : 8 + kIndent;
    RECT rowRect = { indent, y, clientRect.right - 8, y + kRowHeight };

    if (hovered) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, 4, DesignSystem::GetSurfaceHoverColor());
    }

    // Title
    std::wstring title = DesignSystem::Utf8ToWide(bookmark.title.empty() ? bookmark.url : bookmark.title);
    if (title.length() > 35) {
        title = title.substr(0, 32) + L"...";
    }

    RECT textRect = { indent + 8, y, clientRect.right - 12, y + kRowHeight };
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    DesignSystem::DrawTextWithColor(hdc, title, textRect,
        DesignSystem::GetTextPrimaryColor(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

void BookmarksPanel::OnSize(int width, int height) {
    (void)width;
    (void)height;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void BookmarksPanel::OnMouseMove(int x, int y) {
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    HitResult hit = HitTest(x, y);
    if (hit.type != hovered_.type || hit.index != hovered_.index || hit.folder_name != hovered_.folder_name) {
        hovered_ = hit;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void BookmarksPanel::OnLButtonDown(int x, int y) {
    (void)x;
    (void)y;
}

void BookmarksPanel::OnLButtonUp(int x, int y) {
    HitResult hit = HitTest(x, y);

    if (hit.type == HitResult::Folder || hit.type == HitResult::FolderExpand) {
        // Toggle folder expansion
        auto it = std::find(expanded_folders_.begin(), expanded_folders_.end(), hit.folder_name);
        if (it != expanded_folders_.end()) {
            expanded_folders_.erase(it);
        } else {
            expanded_folders_.push_back(hit.folder_name);
        }
        Refresh();
    } else if (hit.type == HitResult::Bookmark && hit.index >= 0) {
        if (static_cast<size_t>(hit.index) < bookmarks_.size()) {
            if (on_open_url_) {
                on_open_url_(bookmarks_[hit.index].url, false);
            }
        }
    }
}

void BookmarksPanel::OnRButtonUp(int x, int y) {
    HitResult hit = HitTest(x, y);
    if (hit.type == HitResult::Bookmark) {
        ShowContextMenu(x, y, hit);
    }
}

void BookmarksPanel::OnMouseWheel(int delta) {
    scroll_offset_ -= delta / 3;
    scroll_offset_ = std::max(0, std::min(scroll_offset_, content_height_ - 100));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void BookmarksPanel::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_.type != HitResult::None) {
        hovered_ = HitResult();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

BookmarksPanel::HitResult BookmarksPanel::HitTest(int x, int y) {
    HitResult result;
    if (y < 40) return result;  // Header area

    int currentY = 40 - scroll_offset_;

    // Check folders
    for (const auto& folder : folders_) {
        if (y >= currentY && y < currentY + kRowHeight) {
            result.type = (x < 32) ? HitResult::FolderExpand : HitResult::Folder;
            result.folder_name = folder;
            return result;
        }
        currentY += kRowHeight;

        // Check bookmarks in folder if expanded
        bool expanded = std::find(expanded_folders_.begin(), expanded_folders_.end(), folder)
                       != expanded_folders_.end();
        if (expanded) {
            for (size_t i = 0; i < bookmarks_.size(); i++) {
                if (bookmarks_[i].folder == folder) {
                    if (y >= currentY && y < currentY + kRowHeight) {
                        result.type = HitResult::Bookmark;
                        result.index = static_cast<int>(i);
                        return result;
                    }
                    currentY += kRowHeight;
                }
            }
        }
    }

    // Check root bookmarks
    for (size_t i = 0; i < bookmarks_.size(); i++) {
        if (bookmarks_[i].folder.empty()) {
            if (y >= currentY && y < currentY + kRowHeight) {
                result.type = HitResult::Bookmark;
                result.index = static_cast<int>(i);
                return result;
            }
            currentY += kRowHeight;
        }
    }

    return result;
}

void BookmarksPanel::ShowContextMenu(int x, int y, const HitResult& hit) {
    if (hit.type != HitResult::Bookmark || hit.index < 0) return;
    if (static_cast<size_t>(hit.index) >= bookmarks_.size()) return;

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Open in New Tab");
    AppendMenuW(menu, MF_STRING, 2, L"Open in Background");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Edit Bookmark");
    AppendMenuW(menu, MF_STRING, 4, L"Delete Bookmark");

    POINT pt = { x, y };
    ClientToScreen(hwnd_, &pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    const Bookmark& bm = bookmarks_[hit.index];

    switch (cmd) {
        case 1:  // Open in New Tab
            if (on_open_url_) on_open_url_(bm.url, false);
            break;
        case 2:  // Open in Background
            if (on_open_url_) on_open_url_(bm.url, true);
            break;
        case 3:  // Edit
            if (on_edit_bookmark_) on_edit_bookmark_(bm.id);
            break;
        case 4:  // Delete
            if (BookmarkStorage* storage = GetBookmarkStorage()) {
                storage->DeleteBookmark(bm.id);
                Refresh();
            }
            break;
    }
}

#endif  // PLATFORM_WIN
