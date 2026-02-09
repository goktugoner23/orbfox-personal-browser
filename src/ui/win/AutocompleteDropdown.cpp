#ifdef PLATFORM_WIN

#include "AutocompleteDropdown.h"
#include "DesignSystem.h"
#include "bookmark_storage.h"
#include "history_storage.h"

#include <windowsx.h>
#include <algorithm>
#include <set>

bool AutocompleteDropdown::class_registered_ = false;

// External accessors for storage
extern BookmarkStorage* GetBookmarkStorage();
extern HistoryStorage* GetHistoryStorage();

AutocompleteDropdown::AutocompleteDropdown() = default;

AutocompleteDropdown::~AutocompleteDropdown() {
    if (font_title_) DeleteObject(font_title_);
    if (font_url_) DeleteObject(font_url_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool AutocompleteDropdown::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
        wcex.lpfnWndProc = AutocompleteDropdown::WndProc;
        wcex.cbWndExtra = sizeof(AutocompleteDropdown*);
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = kClassName;

        if (!RegisterClassExW(&wcex)) return false;
        class_registered_ = true;
    }

    // Create as popup window to appear above other content
    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kClassName,
        nullptr,
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        parent,
        nullptr,
        hInstance,
        this
    );

    return hwnd_ != nullptr;
}

LRESULT CALLBACK AutocompleteDropdown::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AutocompleteDropdown* dropdown = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        dropdown = reinterpret_cast<AutocompleteDropdown*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dropdown));
        dropdown->hwnd_ = hwnd;
    } else {
        dropdown = reinterpret_cast<AutocompleteDropdown*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (dropdown) {
        return dropdown->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT AutocompleteDropdown::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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

        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;

        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_ACTIVATE:
            // If dropdown is being deactivated (clicked elsewhere), hide it
            if (LOWORD(wParam) == WA_INACTIVE) {
                Hide();
            }
            return 0;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void AutocompleteDropdown::OnCreate() {
    font_title_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeBody());
    font_url_ = DesignSystem::MakeFont(DesignSystem::GetFontSizeSmall());
}

void AutocompleteDropdown::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));

    RoundRect(memDC, 0, 0, clientRect.right, clientRect.bottom,
              DesignSystem::GetCornerRadiusMedium() * 2,
              DesignSystem::GetCornerRadiusMedium() * 2);

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(borderPen);

    // Draw suggestions
    int y = kPadding - scroll_offset_;
    for (size_t i = 0; i < suggestions_.size(); i++) {
        if (y + kRowHeight > 0 && y < clientRect.bottom) {
            bool selected = (static_cast<int>(i) == selected_index_);
            bool hovered = (static_cast<int>(i) == hovered_index_);
            DrawSuggestionRow(memDC, suggestions_[i], y, selected, hovered);
        }
        y += kRowHeight;
    }

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void AutocompleteDropdown::DrawSuggestionRow(HDC hdc, const AutocompleteSuggestion& suggestion,
                                              int y, bool selected, bool hovered) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    RECT rowRect = { kPadding, y, clientRect.right - kPadding, y + kRowHeight };

    // Background
    if (selected) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, DesignSystem::GetCornerRadiusSmall(),
                                       DesignSystem::GetAccentColor());
    } else if (hovered) {
        DesignSystem::DrawRoundedRect(hdc, rowRect, DesignSystem::GetCornerRadiusSmall(),
                                       DesignSystem::GetSurfaceHoverColor());
    }

    // Icon area (globe or bookmark icon)
    int iconX = kPadding + DesignSystem::GetSpacingSM();
    int iconY = y + (kRowHeight - kIconSize) / 2;

    HFONT oldFont = (HFONT)SelectObject(hdc, font_title_);

    // Draw icon (bookmark star or globe)
    RECT iconRect = { iconX, iconY, iconX + kIconSize, iconY + kIconSize };
    const wchar_t* icon = suggestion.is_bookmarked ? L"\u2605" : L"\u25CF";  // ★ or ●
    COLORREF iconColor = selected ? RGB(255, 255, 255) :
                         (suggestion.is_bookmarked ? DesignSystem::GetWarningColor() :
                                                     DesignSystem::GetTextTertiaryColor());
    DesignSystem::DrawTextWithColor(hdc, icon, iconRect, iconColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Text area
    int textX = iconX + kIconSize + DesignSystem::GetSpacingSM();
    int textWidth = clientRect.right - textX - kPadding;

    // Title
    RECT titleRect = { textX, y + 6, textX + textWidth, y + 24 };
    std::wstring title = DesignSystem::Utf8ToWide(suggestion.title.empty() ? suggestion.url : suggestion.title);
    COLORREF titleColor = selected ? RGB(255, 255, 255) : DesignSystem::GetTextPrimaryColor();
    DesignSystem::DrawTextWithColor(hdc, title, titleRect, titleColor,
                                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // URL (smaller, secondary color)
    SelectObject(hdc, font_url_);
    RECT urlRect = { textX, y + 24, textX + textWidth, y + kRowHeight - 4 };
    std::wstring url = DesignSystem::Utf8ToWide(suggestion.url);
    COLORREF urlColor = selected ? RGB(220, 220, 255) : DesignSystem::GetAccentColor();
    DesignSystem::DrawTextWithColor(hdc, url, urlRect, urlColor,
                                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldFont);
}

void AutocompleteDropdown::OnSize(int width, int height) {
    (void)width;
    (void)height;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AutocompleteDropdown::OnMouseMove(int x, int y) {
    (void)x;

    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    // Calculate which row is hovered
    int rowY = y + scroll_offset_ - kPadding;
    int newHovered = -1;

    if (rowY >= 0) {
        newHovered = rowY / kRowHeight;
        if (newHovered >= static_cast<int>(suggestions_.size())) {
            newHovered = -1;
        }
    }

    if (newHovered != hovered_index_) {
        hovered_index_ = newHovered;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void AutocompleteDropdown::OnLButtonDown(int x, int y) {
    (void)x;

    // Calculate which row was clicked
    int rowY = y + scroll_offset_ - kPadding;
    if (rowY >= 0) {
        int clickedIndex = rowY / kRowHeight;
        if (clickedIndex >= 0 && clickedIndex < static_cast<int>(suggestions_.size())) {
            selected_index_ = clickedIndex;
            if (on_select_) {
                on_select_(suggestions_[clickedIndex].url);
            }
            Hide();
        }
    }
}

void AutocompleteDropdown::OnMouseWheel(int delta) {
    int maxScroll = std::max(0, static_cast<int>(suggestions_.size()) * kRowHeight -
                             kMaxVisibleRows * kRowHeight);

    scroll_offset_ -= delta / 3;
    scroll_offset_ = std::max(0, std::min(scroll_offset_, maxScroll));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AutocompleteDropdown::OnMouseLeave() {
    tracking_mouse_ = false;
    if (hovered_index_ >= 0) {
        hovered_index_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

std::string AutocompleteDropdown::GetDomainFromURL(const std::string& url) {
    // Parse out the domain from the URL
    std::string domain = url;

    // Remove protocol
    size_t protocolEnd = domain.find("://");
    if (protocolEnd != std::string::npos) {
        domain = domain.substr(protocolEnd + 3);
    }

    // Remove path
    size_t pathStart = domain.find('/');
    if (pathStart != std::string::npos) {
        domain = domain.substr(0, pathStart);
    }

    // Remove port
    size_t portStart = domain.find(':');
    if (portStart != std::string::npos) {
        domain = domain.substr(0, portStart);
    }

    // Convert to lowercase
    std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);

    return domain;
}

void AutocompleteDropdown::UpdateSuggestionsForQuery(const std::string& query) {
    suggestions_.clear();
    scroll_offset_ = 0;

    if (query.empty()) {
        UpdateLayout();
        return;
    }

    // Convert query to lowercase for case-insensitive matching
    std::string lowercaseQuery = query;
    std::transform(lowercaseQuery.begin(), lowercaseQuery.end(), lowercaseQuery.begin(), ::tolower);

    std::set<std::string> seenURLs;

    // Get bookmarks where domain matches query
    BookmarkStorage* bookmarks = GetBookmarkStorage();
    if (bookmarks) {
        std::vector<Bookmark> allBookmarks = bookmarks->GetAllBookmarks();
        for (const auto& bm : allBookmarks) {
            if (suggestions_.size() >= 8) break;

            std::string domain = GetDomainFromURL(bm.url);

            // Only show if domain contains the query
            if (domain.find(lowercaseQuery) == std::string::npos) continue;

            // Skip duplicate URLs
            if (seenURLs.count(bm.url)) continue;
            seenURLs.insert(bm.url);

            AutocompleteSuggestion suggestion;
            suggestion.url = bm.url;
            suggestion.title = bm.title.empty() ? bm.url : bm.title;
            suggestion.is_bookmarked = true;
            suggestions_.push_back(suggestion);
        }
    }

    // Get history entries where domain matches query
    HistoryStorage* history = GetHistoryStorage();
    if (history) {
        std::vector<HistoryEntry> entries = history->SearchHistory(query, 50);

        for (const auto& entry : entries) {
            if (suggestions_.size() >= 8) break;

            std::string domain = GetDomainFromURL(entry.url);

            // Only show if domain contains the query
            if (domain.find(lowercaseQuery) == std::string::npos) continue;

            // Skip duplicate URLs
            if (seenURLs.count(entry.url)) continue;
            seenURLs.insert(entry.url);

            AutocompleteSuggestion suggestion;
            suggestion.url = entry.url;
            suggestion.title = entry.title.empty() ? entry.url : entry.title;
            suggestion.is_bookmarked = false;
            suggestions_.push_back(suggestion);
        }
    }

    // Start with no selection - user must press arrow keys to select
    // This allows Enter to use the typed text instead of first suggestion
    selected_index_ = -1;
    hovered_index_ = -1;

    UpdateLayout();
}

void AutocompleteDropdown::UpdateLayout() {
    if (!hwnd_) return;

    int numRows = std::min(static_cast<int>(suggestions_.size()), kMaxVisibleRows);
    int height = numRows * kRowHeight + kPadding * 2;

    if (suggestions_.empty()) {
        height = 0;
    }

    SetWindowPos(hwnd_, nullptr,
                 pos_x_, pos_y_,
                 width_, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AutocompleteDropdown::SetPosition(int x, int y, int width) {
    pos_x_ = x;
    pos_y_ = y;
    width_ = width;

    if (hwnd_ && IsWindowVisible(hwnd_)) {
        UpdateLayout();
    }
}

void AutocompleteDropdown::Show() {
    if (suggestions_.empty()) {
        Hide();
        return;
    }

    UpdateLayout();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void AutocompleteDropdown::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
    if (on_dismiss_) {
        on_dismiss_();
    }
}

bool AutocompleteDropdown::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void AutocompleteDropdown::MoveSelectionUp() {
    if (suggestions_.empty()) return;

    selected_index_--;
    if (selected_index_ < 0) {
        selected_index_ = static_cast<int>(suggestions_.size()) - 1;
    }

    ScrollToSelection();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AutocompleteDropdown::MoveSelectionDown() {
    if (suggestions_.empty()) return;

    selected_index_++;
    if (selected_index_ >= static_cast<int>(suggestions_.size())) {
        selected_index_ = 0;
    }

    ScrollToSelection();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AutocompleteDropdown::ScrollToSelection() {
    if (selected_index_ < 0) return;

    int rowTop = selected_index_ * kRowHeight;
    int rowBottom = rowTop + kRowHeight;
    int visibleHeight = kMaxVisibleRows * kRowHeight;

    if (rowTop < scroll_offset_) {
        scroll_offset_ = rowTop;
    } else if (rowBottom > scroll_offset_ + visibleHeight) {
        scroll_offset_ = rowBottom - visibleHeight;
    }
}

std::string AutocompleteDropdown::GetSelectedURL() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(suggestions_.size())) {
        return suggestions_[selected_index_].url;
    }
    return "";
}

bool AutocompleteDropdown::HasSelection() const {
    return selected_index_ >= 0 && selected_index_ < static_cast<int>(suggestions_.size());
}

#endif  // PLATFORM_WIN
