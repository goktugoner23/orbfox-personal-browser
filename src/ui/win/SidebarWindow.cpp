#ifdef PLATFORM_WIN

#include "SidebarWindow.h"
#include "DesignSystem.h"
#include "tab_manager.h"
#include "workspace.h"
#include "BookmarksPanel.h"
#include "HistoryPanel.h"
#include "DownloadsPanel.h"
#include "SettingsPanel.h"

#include <windowsx.h>
#include <shlwapi.h>
#include <ole2.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

// Context menu command IDs
namespace {
    constexpr int CMD_CLOSE_TAB = 1001;
    constexpr int CMD_PIN_TAB = 1002;
    constexpr int CMD_MUTE_TAB = 1003;
    constexpr int CMD_DUPLICATE_TAB = 1004;
    constexpr int CMD_RELOAD_TAB = 1005;
    constexpr int CMD_CLOSE_OTHER_TABS = 1006;
    constexpr int CMD_MOVE_TO_WORKSPACE_BASE = 2000;  // 2000 + workspace_id
    constexpr int CMD_MOVE_TO_NEW_WORKSPACE = 3000;
}

bool SidebarWindow::class_registered_ = false;

SidebarWindow::SidebarWindow(TabManager* tab_manager)
    : tab_manager_(tab_manager) {
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplus_token_, &gdiplusStartupInput, nullptr);
}

SidebarWindow::~SidebarWindow() {
    // Kill loading timer if active
    if (hwnd_) {
        KillTimer(hwnd_, kLoadingTimerId);
    }

    // Destroy rename edit if active
    if (rename_edit_) {
        DestroyWindow(rename_edit_);
        rename_edit_ = nullptr;
    }

    ClearFaviconCache();

    if (font_normal_) DeleteObject(font_normal_);
    if (font_bold_) DeleteObject(font_bold_);
    if (font_small_) DeleteObject(font_small_);
    if (font_icon_) DeleteObject(font_icon_);
    if (hwnd_) DestroyWindow(hwnd_);

    // Shutdown GDI+
    if (gdiplus_token_) {
        Gdiplus::GdiplusShutdown(gdiplus_token_);
    }
}

bool SidebarWindow::RegisterWindowClass(HINSTANCE hInstance) {
    if (class_registered_) return true;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // Enable double-click messages
    wcex.lpfnWndProc = SidebarWindow::WndProc;
    wcex.cbWndExtra = sizeof(SidebarWindow*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;  // We paint our own background
    wcex.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wcex)) return false;

    class_registered_ = true;
    return true;
}

bool SidebarWindow::Create(HWND parent) {
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

LRESULT CALLBACK SidebarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SidebarWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<SidebarWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<SidebarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT SidebarWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_DESTROY:
            OnDestroy();
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

        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MBUTTONUP:
            OnMButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_RBUTTONUP:
            OnRButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDBLCLK:
            OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd_, &pt);
            OnMouseWheel(pt.x, pt.y, GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        }

        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;

        case WM_TIMER:
            OnTimer(wParam);
            return 0;

        case WM_COMMAND:
            OnContextMenuCommand(LOWORD(wParam));
            return 0;

        case WM_ERASEBKGND:
            return 1;  // Prevent flicker
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void SidebarWindow::OnCreate() {
    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_bold_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody(), FW_SEMIBOLD);
    font_small_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeSmall());
    // Create icon font using Segoe UI Symbol for emoji rendering
    font_icon_ = CreateFontW(
        -MulDiv(12, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI Symbol"
    );

    // Create panels
    bookmarks_panel_ = std::make_unique<BookmarksPanel>();
    bookmarks_panel_->Create(hwnd_);

    history_panel_ = std::make_unique<HistoryPanel>();
    history_panel_->Create(hwnd_);

    downloads_panel_ = std::make_unique<DownloadsPanel>();
    downloads_panel_->Create(hwnd_);

    settings_panel_ = std::make_unique<SettingsPanel>();
    settings_panel_->Create(hwnd_);

    // Set up callbacks
    bookmarks_panel_->SetOpenUrlCallback([this](const std::string& url, bool bg) {
        if (on_open_url_) on_open_url_(url, bg);
    });

    history_panel_->SetOpenUrlCallback([this](const std::string& url, bool bg) {
        if (on_open_url_) on_open_url_(url, bg);
    });

    settings_panel_->SetOpenUrlCallback([this](const std::string& url, bool bg) {
        if (on_open_url_) on_open_url_(url, bg);
    });

    // Start loading animation timer (for tabs that are loading)
    SetTimer(hwnd_, kLoadingTimerId, 100, nullptr);
}

void SidebarWindow::OnDestroy() {
    KillTimer(hwnd_, kLoadingTimerId);
}

void SidebarWindow::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    // Double buffering
    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw border on right edge
    HPEN borderPen = CreatePen(PS_SOLID, 1, DesignSystem::GetBorderColor());
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    MoveToEx(memDC, clientRect.right - 1, 0, nullptr);
    LineTo(memDC, clientRect.right - 1, clientRect.bottom);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Draw components
    DrawPanelIcons(memDC);
    DrawWorkspaceTabs(memDC);
    DrawAddWorkspaceButton(memDC);
    DrawTabList(memDC);
    DrawNewTabButton(memDC);

    // Copy to screen
    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void SidebarWindow::OnSize(int width, int height) {
    (void)width;
    (void)height;

    // Resize visible panel
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int panelX = panelWidth;
    int panelW = clientRect.right - panelWidth;
    int panelH = clientRect.bottom;

    if (bookmarks_panel_ && bookmarks_panel_->IsVisible()) {
        SetWindowPos(bookmarks_panel_->GetHWND(), nullptr, panelX, 0, panelW, panelH, SWP_NOZORDER);
    }
    if (history_panel_ && history_panel_->IsVisible()) {
        SetWindowPos(history_panel_->GetHWND(), nullptr, panelX, 0, panelW, panelH, SWP_NOZORDER);
    }
    if (downloads_panel_ && downloads_panel_->IsVisible()) {
        SetWindowPos(downloads_panel_->GetHWND(), nullptr, panelX, 0, panelW, panelH, SWP_NOZORDER);
    }
    if (settings_panel_ && settings_panel_->IsVisible()) {
        SetWindowPos(settings_panel_->GetHWND(), nullptr, panelX, 0, panelW, panelH, SWP_NOZORDER);
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SidebarWindow::OnMouseMove(int x, int y) {
    // Track mouse for WM_MOUSELEAVE
    if (!tracking_mouse_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        tracking_mouse_ = true;
    }

    // Handle workspace dragging
    if (is_dragging_workspace_) {
        UpdateWorkspaceDrag(x, y);
        return;
    }

    bool needs_repaint = false;

    // Check panel icon hover
    int icon = HitTestPanelIcon(x, y);
    if (icon != hovered_panel_icon_) {
        hovered_panel_icon_ = icon;
        needs_repaint = true;
    }

    // Check workspace tab hover
    int ws = HitTestWorkspaceTab(x, y);
    if (ws != hovered_workspace_) {
        hovered_workspace_ = ws;
        needs_repaint = true;
    }

    // Check workspace close button hover
    int wsClose = -1;
    if (hovered_workspace_ >= 0) {
        if (HitTestWorkspaceCloseButton(x, y, hovered_workspace_)) {
            wsClose = hovered_workspace_;
        }
    }
    if (wsClose != hovered_workspace_close_) {
        hovered_workspace_close_ = wsClose;
        needs_repaint = true;
    }

    // Check add workspace button hover
    bool addWs = HitTestAddWorkspaceButton(x, y);
    if (addWs != hovered_add_workspace_) {
        hovered_add_workspace_ = addWs;
        needs_repaint = true;
    }

    // Check tab row hover
    int tab = HitTestTabRow(x, y);
    if (tab != hovered_tab_) {
        hovered_tab_ = tab;
        needs_repaint = true;
    }

    // Check close button hover (only when hovering a tab)
    bool closeButton = false;
    if (hovered_tab_ >= 0) {
        closeButton = HitTestTabCloseButton(x, y, hovered_tab_);
    }
    if (closeButton != hovered_close_button_) {
        hovered_close_button_ = closeButton;
        needs_repaint = true;
    }

    // Check new tab button hover
    bool newTab = HitTestNewTabButton(x, y);
    if (newTab != hovered_new_tab_) {
        hovered_new_tab_ = newTab;
        needs_repaint = true;
    }

    if (needs_repaint) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SidebarWindow::OnLButtonDown(int x, int y) {
    // Check if clicking on a workspace tab to potentially start drag
    int ws = HitTestWorkspaceTab(x, y);
    if (ws >= 0 && !HitTestWorkspaceCloseButton(x, y, ws)) {
        StartWorkspaceDrag(ws, x, y);
    }
}

void SidebarWindow::OnLButtonUp(int x, int y) {
    // End workspace drag if active
    if (is_dragging_workspace_) {
        EndWorkspaceDrag();
        return;
    }

    // Handle panel icon click
    int icon = HitTestPanelIcon(x, y);
    if (icon >= 0) {
        ShowPanel(static_cast<Panel>(icon));
        return;
    }

    // Handle add workspace button click
    if (HitTestAddWorkspaceButton(x, y)) {
        AddNewWorkspace();
        return;
    }

    // Handle workspace tab click
    int ws = HitTestWorkspaceTab(x, y);
    if (ws >= 0 && tab_manager_) {
        // Check if close button was clicked
        if (HitTestWorkspaceCloseButton(x, y, ws)) {
            const auto& workspaces = tab_manager_->GetWorkspaces();
            if (ws < static_cast<int>(workspaces.size())) {
                DeleteWorkspace(workspaces[ws]->id);
            }
            return;
        }

        // Otherwise select the workspace
        const auto& workspaces = tab_manager_->GetWorkspaces();
        if (ws < static_cast<int>(workspaces.size())) {
            tab_manager_->SetActiveWorkspace(workspaces[ws]->id);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return;
    }

    // Handle tab row click
    int tab = HitTestTabRow(x, y);
    if (tab >= 0) {
        // Check if close button was clicked
        if (HitTestTabCloseButton(x, y, tab)) {
            Workspace* activeWs = tab_manager_ ? tab_manager_->GetActiveWorkspace() : nullptr;
            if (activeWs && tab < static_cast<int>(activeWs->tabs.size()) && on_tab_close_) {
                on_tab_close_(activeWs->tabs[tab]->id);
            }
            return;
        }

        // Otherwise select the tab
        if (on_tab_selected_) {
            Workspace* activeWs = tab_manager_ ? tab_manager_->GetActiveWorkspace() : nullptr;
            if (activeWs && tab < static_cast<int>(activeWs->tabs.size())) {
                on_tab_selected_(activeWs->tabs[tab]->id);
            }
        }
        return;
    }

    // Handle new tab button click
    if (HitTestNewTabButton(x, y) && on_new_tab_) {
        on_new_tab_();
        return;
    }
}

void SidebarWindow::OnLButtonDblClk(int x, int y) {
    // Double-click on workspace tab to rename inline
    int ws = HitTestWorkspaceTab(x, y);
    if (ws >= 0) {
        StartInlineRename(ws);
    }
}

void SidebarWindow::OnMButtonUp(int x, int y) {
    // Middle-click on workspace tab to close it
    int ws = HitTestWorkspaceTab(x, y);
    if (ws >= 0 && tab_manager_) {
        const auto& workspaces = tab_manager_->GetWorkspaces();
        if (ws < static_cast<int>(workspaces.size())) {
            DeleteWorkspace(workspaces[ws]->id);
        }
        return;
    }

    // Middle-click on tab to close it
    int tab = HitTestTabRow(x, y);
    if (tab >= 0 && on_tab_close_) {
        Workspace* activeWs = tab_manager_ ? tab_manager_->GetActiveWorkspace() : nullptr;
        if (activeWs && tab < static_cast<int>(activeWs->tabs.size())) {
            on_tab_close_(activeWs->tabs[tab]->id);
        }
    }
}

void SidebarWindow::OnRButtonUp(int x, int y) {
    // Right-click on workspace tab to show context menu
    int ws = HitTestWorkspaceTab(x, y);
    if (ws >= 0) {
        ShowWorkspaceContextMenu(x, y, ws);
        return;
    }

    // Right-click on tab to show context menu
    int tab = HitTestTabRow(x, y);
    if (tab >= 0) {
        ShowTabContextMenu(x, y, tab);
    }
}

void SidebarWindow::OnMouseWheel(int x, int y, int delta) {
    // Scroll workspace tabs horizontally if mouse is in workspace area
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;

    if (y < workspace_tab_height_ + DesignSystem::GetSpacingMD() && x > panelWidth) {
        // Calculate available width for workspace tabs
        int availableWidth = clientRect.right - panelWidth - add_workspace_btn_size_ - DesignSystem::GetSpacingMD() * 2;

        if (workspace_total_width_ > availableWidth) {
            int maxScroll = workspace_total_width_ - availableWidth;
            workspace_scroll_offset_ -= delta / 3;  // Scroll sensitivity
            workspace_scroll_offset_ = std::max(0, std::min(workspace_scroll_offset_, maxScroll));
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void SidebarWindow::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kLoadingTimerId) {
        // Advance loading animation frame
        loading_animation_frame_ = (loading_animation_frame_ + 1) % 8;

        // Check if any tabs are loading and need repaint
        Workspace* activeWs = tab_manager_ ? tab_manager_->GetActiveWorkspace() : nullptr;
        if (activeWs) {
            bool hasLoadingTab = false;
            for (const auto& tab : activeWs->tabs) {
                if (tab->is_loading) {
                    hasLoadingTab = true;
                    break;
                }
            }
            if (hasLoadingTab) {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        }
    }
}

void SidebarWindow::OnMouseLeave() {
    tracking_mouse_ = false;
    bool needs_repaint = (hovered_panel_icon_ >= 0 || hovered_workspace_ >= 0 ||
                          hovered_workspace_close_ >= 0 || hovered_add_workspace_ ||
                          hovered_tab_ >= 0 || hovered_close_button_ || hovered_new_tab_);
    hovered_panel_icon_ = -1;
    hovered_workspace_ = -1;
    hovered_workspace_close_ = -1;
    hovered_add_workspace_ = false;
    hovered_tab_ = -1;
    hovered_close_button_ = false;
    hovered_new_tab_ = false;

    if (needs_repaint) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SidebarWindow::DrawPanelIcons(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int x = panel_icon_margin_;
    int y = panel_icon_margin_;

    // Panel icons: Tabs, Bookmarks, History, Downloads, Settings
    const wchar_t* icons[] = { L"\U0001F4D1", L"\u2B50", L"\U0001F552", L"\u2B07", L"\u2699" };
    const int iconCount = 5;

    for (int i = 0; i < iconCount; i++) {
        RECT iconRect = { x, y, x + panel_icon_size_, y + panel_icon_size_ };

        // Draw hover/active background
        COLORREF bgColor = DesignSystem::GetSurfaceColor();
        if (i == static_cast<int>(current_panel_)) {
            bgColor = DesignSystem::GetSurfaceActiveColor();
        } else if (i == hovered_panel_icon_) {
            bgColor = DesignSystem::GetSurfaceHoverColor();
        }

        DesignSystem::DrawRoundedRect(hdc, iconRect, DesignSystem::GetCornerRadiusSmall(), bgColor);

        // Draw icon (using text for now - should use proper icons)
        HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
        DesignSystem::DrawTextWithColor(hdc, icons[i], iconRect, DesignSystem::GetTextSecondaryColor(),
                                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);

        y += panel_icon_size_ + panel_icon_margin_;
    }
}

void SidebarWindow::DrawWorkspaceTabs(HDC hdc) {
    if (!tab_manager_) return;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int startX = panelWidth + DesignSystem::GetSpacingSM();
    int y = DesignSystem::GetSpacingSM();
    // Reserve space for the add workspace button
    int availableWidth = clientRect.right - panelWidth - add_workspace_btn_size_ - DesignSystem::GetSpacingMD() * 2;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    Workspace* activeWs = tab_manager_->GetActiveWorkspace();

    // Calculate tab width - use workspace color from the workspace's color property
    int tabWidth = GetWorkspaceTabWidth();

    // Calculate total width for scrolling
    workspace_total_width_ = static_cast<int>(workspaces.size()) * tabWidth;

    // Apply scroll offset
    int x = startX - workspace_scroll_offset_;

    // Set up clipping region for workspace tabs area
    HRGN clipRegion = CreateRectRgn(startX, y, startX + availableWidth, y + workspace_tab_height_);
    SelectClipRgn(hdc, clipRegion);

    HFONT oldFont = (HFONT)SelectObject(hdc, font_bold_);

    for (size_t i = 0; i < workspaces.size(); i++) {
        const auto& ws = workspaces[i];

        // Skip if completely outside visible area
        if (x + tabWidth < startX || x > startX + availableWidth) {
            x += tabWidth;
            continue;
        }

        RECT tabRect = { x, y, x + tabWidth - DesignSystem::GetSpacingXS(), y + workspace_tab_height_ };

        // Check if this is the workspace being dragged
        bool isDragging = is_dragging_workspace_ && static_cast<int>(i) == drag_workspace_index_;

        // Background
        COLORREF bgColor = DesignSystem::GetSurfaceColor();
        if (isDragging) {
            bgColor = DesignSystem::GetAccentColor();
        } else if (activeWs && ws->id == activeWs->id) {
            bgColor = DesignSystem::GetSurfaceActiveColor();
        } else if (static_cast<int>(i) == hovered_workspace_) {
            bgColor = DesignSystem::GetSurfaceHoverColor();
        }

        DesignSystem::DrawRoundedRect(hdc, tabRect, DesignSystem::GetCornerRadiusSmall(), bgColor);

        // Workspace color indicator (8px diameter circle)
        COLORREF wsColor = DesignSystem::GetWorkspaceColor(static_cast<int>(i));
        // Parse workspace color if set
        if (!ws->color.empty() && ws->color[0] == '#') {
            unsigned int r = 0, g = 0, b = 0;
            if (sscanf(ws->color.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                wsColor = RGB(r, g, b);
            }
        }
        int dotSize = 8;
        int dotY = tabRect.top + (workspace_tab_height_ - dotSize) / 2;
        RECT colorRect = { tabRect.left + 6, dotY, tabRect.left + 6 + dotSize, dotY + dotSize };
        DesignSystem::DrawRoundedRect(hdc, colorRect, dotSize / 2, wsColor);

        // Close button (X) on hover - only show when hovering this workspace
        bool showClose = (static_cast<int>(i) == hovered_workspace_) && !isDragging;
        int closeSize = 16;
        int textRightPadding = showClose ? closeSize + 4 : 4;

        if (showClose) {
            int closeX = tabRect.right - closeSize - 4;
            int closeY = tabRect.top + (workspace_tab_height_ - closeSize) / 2;
            RECT closeRect = { closeX, closeY, closeX + closeSize, closeY + closeSize };

            // Hover background for close button
            if (static_cast<int>(i) == hovered_workspace_close_) {
                DesignSystem::DrawRoundedRect(hdc, closeRect, DesignSystem::GetCornerRadiusSmall(),
                                               DesignSystem::GetErrorColor());
            }

            // Draw X
            HFONT smallFont = (HFONT)SelectObject(hdc, font_small_);
            COLORREF closeColor = (static_cast<int>(i) == hovered_workspace_close_)
                ? DesignSystem::GetTextPrimaryColor()
                : DesignSystem::GetTextSecondaryColor();
            DesignSystem::DrawTextWithColor(hdc, L"\u00D7", closeRect, closeColor,
                                             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, smallFont);
        }

        // Workspace name
        RECT textRect = { tabRect.left + 6 + dotSize + 4, tabRect.top,
                          tabRect.right - textRightPadding, tabRect.bottom };
        std::wstring name = DesignSystem::Utf8ToWide(ws->name);
        DesignSystem::DrawTextWithColor(hdc, name, textRect, DesignSystem::GetTextPrimaryColor(),
                                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Draw drag insert indicator
        if (is_dragging_workspace_ && static_cast<int>(i) == drag_insert_index_) {
            HPEN indicatorPen = CreatePen(PS_SOLID, 2, DesignSystem::GetAccentColor());
            HPEN oldPen = (HPEN)SelectObject(hdc, indicatorPen);
            MoveToEx(hdc, tabRect.left, tabRect.top + 2, nullptr);
            LineTo(hdc, tabRect.left, tabRect.bottom - 2);
            SelectObject(hdc, oldPen);
            DeleteObject(indicatorPen);
        }

        x += tabWidth;
    }

    // Draw insert indicator at end if needed
    if (is_dragging_workspace_ && drag_insert_index_ == static_cast<int>(workspaces.size())) {
        int indicatorX = x - DesignSystem::GetSpacingXS();
        HPEN indicatorPen = CreatePen(PS_SOLID, 2, DesignSystem::GetAccentColor());
        HPEN oldPen = (HPEN)SelectObject(hdc, indicatorPen);
        MoveToEx(hdc, indicatorX, y + 2, nullptr);
        LineTo(hdc, indicatorX, y + workspace_tab_height_ - 2);
        SelectObject(hdc, oldPen);
        DeleteObject(indicatorPen);
    }

    SelectObject(hdc, oldFont);

    // Reset clipping region
    SelectClipRgn(hdc, nullptr);
    DeleteObject(clipRegion);
}

void SidebarWindow::DrawAddWorkspaceButton(HDC hdc) {
    RECT btnRect = GetAddWorkspaceButtonRect();

    // Background
    COLORREF bgColor = hovered_add_workspace_
        ? DesignSystem::GetSurfaceHoverColor()
        : DesignSystem::GetSurfaceColor();
    DesignSystem::DrawRoundedRect(hdc, btnRect, DesignSystem::GetCornerRadiusSmall(), bgColor,
                                   DesignSystem::GetBorderSubtleColor());

    // Plus icon
    HFONT oldFont = (HFONT)SelectObject(hdc, font_bold_);
    DesignSystem::DrawTextWithColor(hdc, L"+", btnRect, DesignSystem::GetTextSecondaryColor(),
                                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

void SidebarWindow::DrawTabList(HDC hdc) {
    if (!tab_manager_) return;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int x = panelWidth + DesignSystem::GetSpacingSM();
    int y = workspace_tab_height_ + DesignSystem::GetSpacingMD() * 2;
    int width = clientRect.right - panelWidth - DesignSystem::GetSpacingMD();

    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    if (!activeWs) return;

    Tab* activeTab = tab_manager_->GetActiveTab();
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);

    // Create GDI+ Graphics for drawing favicons
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    const int iconSize = DesignSystem::GetIconSizeSmall();
    const int closeButtonSize = 18;
    const int statusIconSize = 14;

    for (size_t i = 0; i < activeWs->tabs.size(); i++) {
        const auto& tab = activeWs->tabs[i];
        RECT rowRect = { x, y, x + width, y + tab_row_height_ };
        bool isHovered = (static_cast<int>(i) == hovered_tab_);
        bool isActive = (activeTab && tab->id == activeTab->id);

        // Background
        COLORREF bgColor = DesignSystem::GetSurfaceColor();
        if (isActive) {
            bgColor = DesignSystem::GetSurfaceActiveColor();
        } else if (isHovered) {
            bgColor = DesignSystem::GetSurfaceHoverColor();
        }

        DesignSystem::DrawRoundedRect(hdc, rowRect, DesignSystem::GetCornerRadiusSmall(), bgColor);

        int contentX = rowRect.left + DesignSystem::GetSpacingSM();
        int centerY = rowRect.top + (tab_row_height_ - iconSize) / 2;

        // Draw favicon or loading spinner
        if (tab->is_loading) {
            // Draw loading spinner (rotating arc)
            Gdiplus::Pen pen(Gdiplus::Color(255, 59, 130, 246), 2.0f);
            float startAngle = loading_animation_frame_ * 45.0f;
            graphics.DrawArc(&pen, contentX, centerY, iconSize, iconSize, startAngle, 270.0f);
            contentX += iconSize + DesignSystem::GetSpacingXS();
        } else {
            // Draw favicon if available
            Gdiplus::Bitmap* favicon = GetCachedFavicon(tab->id);
            if (!favicon && !tab->favicon_data.empty()) {
                UpdateFaviconCache(tab->id, tab->favicon_data);
                favicon = GetCachedFavicon(tab->id);
            }
            if (favicon) {
                graphics.DrawImage(favicon, contentX, centerY, iconSize, iconSize);
            }
            contentX += iconSize + DesignSystem::GetSpacingXS();
        }

        // Calculate right side icons area
        int rightX = rowRect.right - DesignSystem::GetSpacingSM();

        // Close button (shown on hover)
        if (isHovered) {
            int closeBtnX = rightX - closeButtonSize;
            int closeBtnY = rowRect.top + (tab_row_height_ - closeButtonSize) / 2;
            RECT closeBtnRect = { closeBtnX, closeBtnY, closeBtnX + closeButtonSize, closeBtnY + closeButtonSize };
            if (hovered_close_button_) {
                DesignSystem::DrawRoundedRect(hdc, closeBtnRect, 4, DesignSystem::GetSurfaceActiveColor());
            }
            COLORREF closeColor = hovered_close_button_ ? DesignSystem::GetTextPrimaryColor() : DesignSystem::GetTextSecondaryColor();
            DesignSystem::DrawTextWithColor(hdc, L"\u00D7", closeBtnRect, closeColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightX -= closeButtonSize + DesignSystem::GetSpacingXS();
        }

        // Status icons
        HFONT iconFont = (HFONT)SelectObject(hdc, font_icon_);

        // Pin indicator
        if (tab->is_pinned && !isHovered) {
            int pinX = rightX - statusIconSize;
            int pinY = rowRect.top + (tab_row_height_ - statusIconSize) / 2;
            RECT pinRect = { pinX, pinY, pinX + statusIconSize + 2, pinY + statusIconSize };
            DesignSystem::DrawTextWithColor(hdc, L"\U0001F4CC", pinRect, DesignSystem::GetTextSecondaryColor(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightX -= statusIconSize + DesignSystem::GetSpacingXS();
        }

        // Mute indicator
        if (tab->is_muted) {
            int muteX = rightX - statusIconSize;
            int muteY = rowRect.top + (tab_row_height_ - statusIconSize) / 2;
            RECT muteRect = { muteX, muteY, muteX + statusIconSize + 2, muteY + statusIconSize };
            DesignSystem::DrawTextWithColor(hdc, L"\U0001F507", muteRect, DesignSystem::GetTextSecondaryColor(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightX -= statusIconSize + DesignSystem::GetSpacingXS();
        }

        // Hibernate indicator (moon)
        if (tab->is_hibernated) {
            int moonX = rightX - statusIconSize;
            int moonY = rowRect.top + (tab_row_height_ - statusIconSize) / 2;
            RECT moonRect = { moonX, moonY, moonX + statusIconSize + 2, moonY + statusIconSize };
            DesignSystem::DrawTextWithColor(hdc, L"\U0001F319", moonRect, DesignSystem::GetTextSecondaryColor(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightX -= statusIconSize + DesignSystem::GetSpacingXS();
        }

        SelectObject(hdc, font_normal_);

        // Tab title
        RECT textRect = { contentX, rowRect.top, rightX - DesignSystem::GetSpacingXS(), rowRect.bottom };
        std::wstring title = tab->title.empty() ? DesignSystem::Utf8ToWide(tab->url) : DesignSystem::Utf8ToWide(tab->title);
        COLORREF textColor = tab->is_hibernated ? DesignSystem::GetTextSecondaryColor() : DesignSystem::GetTextPrimaryColor();
        DesignSystem::DrawTextWithColor(hdc, title, textRect, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += tab_row_height_ + DesignSystem::GetSpacingXS();
    }

    SelectObject(hdc, oldFont);
}

void SidebarWindow::DrawNewTabButton(HDC hdc) {
    if (!tab_manager_) return;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int x = panelWidth + DesignSystem::GetSpacingSM();
    int width = clientRect.right - panelWidth - DesignSystem::GetSpacingMD();

    // Calculate Y position after tab list
    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    int tabCount = activeWs ? static_cast<int>(activeWs->tabs.size()) : 0;
    int y = workspace_tab_height_ + DesignSystem::GetSpacingMD() * 2 +
            tabCount * (tab_row_height_ + DesignSystem::GetSpacingXS());

    RECT buttonRect = { x, y, x + width, y + tab_row_height_ };

    // Background
    COLORREF bgColor = hovered_new_tab_ ? DesignSystem::GetSurfaceHoverColor() : DesignSystem::GetSurfaceColor();
    DesignSystem::DrawRoundedRect(hdc, buttonRect, DesignSystem::GetCornerRadiusSmall(), bgColor,
                                   DesignSystem::GetBorderSubtleColor());

    // Text
    HFONT oldFont = (HFONT)SelectObject(hdc, font_normal_);
    DesignSystem::DrawTextWithColor(hdc, L"+ New Tab", buttonRect, DesignSystem::GetTextSecondaryColor(),
                                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

int SidebarWindow::HitTestPanelIcon(int x, int y) {
    int iconX = panel_icon_margin_;
    int iconY = panel_icon_margin_;

    for (int i = 0; i < 5; i++) {
        RECT iconRect = { iconX, iconY, iconX + panel_icon_size_, iconY + panel_icon_size_ };
        POINT pt = { x, y };
        if (PtInRect(&iconRect, pt)) {
            return i;
        }
        iconY += panel_icon_size_ + panel_icon_margin_;
    }

    return -1;
}

int SidebarWindow::HitTestWorkspaceTab(int x, int y) {
    if (!tab_manager_) return -1;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspaces.empty()) return -1;

    int tabWidth = GetWorkspaceTabWidth();

    for (size_t i = 0; i < workspaces.size(); i++) {
        RECT tabRect = GetWorkspaceTabRect(static_cast<int>(i));
        POINT pt = { x, y };
        if (PtInRect(&tabRect, pt)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool SidebarWindow::HitTestWorkspaceCloseButton(int x, int y, int workspace_index) {
    RECT closeRect = GetWorkspaceCloseButtonRect(workspace_index);
    POINT pt = { x, y };
    return PtInRect(&closeRect, pt);
}

bool SidebarWindow::HitTestAddWorkspaceButton(int x, int y) {
    RECT btnRect = GetAddWorkspaceButtonRect();
    POINT pt = { x, y };
    return PtInRect(&btnRect, pt);
}

RECT SidebarWindow::GetWorkspaceTabRect(int workspace_index) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int startX = panelWidth + DesignSystem::GetSpacingSM();
    int tabY = DesignSystem::GetSpacingSM();
    int tabWidth = GetWorkspaceTabWidth();

    int tabX = startX - workspace_scroll_offset_ + workspace_index * tabWidth;

    RECT tabRect = { tabX, tabY, tabX + tabWidth - DesignSystem::GetSpacingXS(), tabY + workspace_tab_height_ };
    return tabRect;
}

RECT SidebarWindow::GetWorkspaceCloseButtonRect(int workspace_index) {
    RECT tabRect = GetWorkspaceTabRect(workspace_index);
    int closeSize = 16;
    int closeX = tabRect.right - closeSize - 4;
    int closeY = tabRect.top + (workspace_tab_height_ - closeSize) / 2;
    RECT closeRect = { closeX, closeY, closeX + closeSize, closeY + closeSize };
    return closeRect;
}

RECT SidebarWindow::GetAddWorkspaceButtonRect() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int btnX = clientRect.right - add_workspace_btn_size_ - DesignSystem::GetSpacingSM();
    int btnY = DesignSystem::GetSpacingSM() + (workspace_tab_height_ - add_workspace_btn_size_) / 2;

    RECT btnRect = { btnX, btnY, btnX + add_workspace_btn_size_, btnY + add_workspace_btn_size_ };
    return btnRect;
}

int SidebarWindow::GetWorkspaceTabWidth() {
    if (!tab_manager_) return workspace_tab_max_width_;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int availableWidth = clientRect.right - panelWidth - add_workspace_btn_size_ - DesignSystem::GetSpacingMD() * 2;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspaces.empty()) return workspace_tab_max_width_;

    int tabWidth = availableWidth / static_cast<int>(workspaces.size());
    tabWidth = std::max(workspace_tab_min_width_, std::min(tabWidth, workspace_tab_max_width_));
    return tabWidth;
}

int SidebarWindow::HitTestTabRow(int x, int y) {
    if (!tab_manager_) return -1;

    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    if (!activeWs) return -1;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int rowX = panelWidth + DesignSystem::GetSpacingSM();
    int rowY = workspace_tab_height_ + DesignSystem::GetSpacingMD() * 2;
    int width = clientRect.right - panelWidth - DesignSystem::GetSpacingMD();

    for (size_t i = 0; i < activeWs->tabs.size(); i++) {
        RECT rowRect = { rowX, rowY, rowX + width, rowY + tab_row_height_ };
        POINT pt = { x, y };
        if (PtInRect(&rowRect, pt)) {
            return static_cast<int>(i);
        }
        rowY += tab_row_height_ + DesignSystem::GetSpacingXS();
    }

    return -1;
}

bool SidebarWindow::HitTestNewTabButton(int x, int y) {
    if (!tab_manager_) return false;

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int btnX = panelWidth + DesignSystem::GetSpacingSM();
    int width = clientRect.right - panelWidth - DesignSystem::GetSpacingMD();

    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    int tabCount = activeWs ? static_cast<int>(activeWs->tabs.size()) : 0;
    int btnY = workspace_tab_height_ + DesignSystem::GetSpacingMD() * 2 +
               tabCount * (tab_row_height_ + DesignSystem::GetSpacingXS());

    RECT buttonRect = { btnX, btnY, btnX + width, btnY + tab_row_height_ };
    POINT pt = { x, y };
    return PtInRect(&buttonRect, pt);
}

bool SidebarWindow::HitTestTabCloseButton(int x, int y, int tab_index) {
    RECT closeRect = GetTabCloseButtonRect(tab_index);
    POINT pt = { x, y };
    return PtInRect(&closeRect, pt);
}

RECT SidebarWindow::GetTabRowRect(int tab_index) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int x = panelWidth + DesignSystem::GetSpacingSM();
    int y = workspace_tab_height_ + DesignSystem::GetSpacingMD() * 2 +
            tab_index * (tab_row_height_ + DesignSystem::GetSpacingXS());
    int width = clientRect.right - panelWidth - DesignSystem::GetSpacingMD();

    RECT rowRect = { x, y, x + width, y + tab_row_height_ };
    return rowRect;
}

RECT SidebarWindow::GetTabCloseButtonRect(int tab_index) {
    RECT rowRect = GetTabRowRect(tab_index);
    int closeSize = 20;
    int closeX = rowRect.right - closeSize - 8;
    int closeY = rowRect.top + (tab_row_height_ - closeSize) / 2;
    RECT closeRect = { closeX, closeY, closeX + closeSize, closeY + closeSize };
    return closeRect;
}

void SidebarWindow::ShowTabContextMenu(int x, int y, int tab_index) {
    if (!tab_manager_) return;

    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    if (!activeWs || tab_index < 0 || tab_index >= static_cast<int>(activeWs->tabs.size())) return;

    context_menu_tab_index_ = tab_index;
    const auto& tab = activeWs->tabs[tab_index];

    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, CMD_CLOSE_TAB, L"Close Tab");
    AppendMenuW(menu, MF_STRING, CMD_DUPLICATE_TAB, L"Duplicate Tab");
    AppendMenuW(menu, MF_STRING, CMD_RELOAD_TAB, L"Reload Tab");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Pin/Unpin option
    if (tab->is_pinned) {
        AppendMenuW(menu, MF_STRING, CMD_PIN_TAB, L"Unpin Tab");
    } else {
        AppendMenuW(menu, MF_STRING, CMD_PIN_TAB, L"Pin Tab");
    }

    // Mute/Unmute option
    if (tab->is_muted) {
        AppendMenuW(menu, MF_STRING, CMD_MUTE_TAB, L"Unmute Tab");
    } else {
        AppendMenuW(menu, MF_STRING, CMD_MUTE_TAB, L"Mute Tab");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_CLOSE_OTHER_TABS, L"Close Other Tabs");

    // Move to workspace submenu
    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspaces.size() > 1 || true) {
        HMENU moveMenu = CreatePopupMenu();
        for (const auto& ws : workspaces) {
            if (ws.get() != activeWs) {
                std::wstring name = DesignSystem::Utf8ToWide(ws->name);
                AppendMenuW(moveMenu, MF_STRING, CMD_MOVE_TO_WORKSPACE_BASE + ws->id, name.c_str());
            }
        }
        AppendMenuW(moveMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(moveMenu, MF_STRING, CMD_MOVE_TO_NEW_WORKSPACE, L"New Workspace...");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)moveMenu, L"Move to Workspace");
    }

    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);

    DestroyMenu(menu);
}

void SidebarWindow::OnContextMenuCommand(int cmd_id) {
    // Check if it's a workspace context menu command
    if (cmd_id >= IDM_WORKSPACE_RENAME && cmd_id <= IDM_WORKSPACE_COLOR_BASE + 8) {
        OnWorkspaceContextMenuCommand(cmd_id);
        return;
    }

    // Handle tab context menu commands
    if (context_menu_tab_index_ < 0 || !tab_manager_) return;

    Workspace* activeWs = tab_manager_->GetActiveWorkspace();
    if (!activeWs || context_menu_tab_index_ >= static_cast<int>(activeWs->tabs.size())) return;

    int tab_id = activeWs->tabs[context_menu_tab_index_]->id;

    Tab* tab = activeWs->tabs[context_menu_tab_index_].get();

    switch (cmd_id) {
        case CMD_CLOSE_TAB:
            if (on_tab_close_) on_tab_close_(tab_id);
            break;

        case CMD_DUPLICATE_TAB:
            if (on_tab_duplicate_) on_tab_duplicate_(tab_id);
            break;

        case CMD_RELOAD_TAB:
            // TODO: Implement reload callback
            break;

        case CMD_PIN_TAB:
            // Toggle pin state
            tab->is_pinned = !tab->is_pinned;
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case CMD_MUTE_TAB:
            // Toggle mute state
            tab->is_muted = !tab->is_muted;
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case CMD_CLOSE_OTHER_TABS:
            // Close all tabs except the selected one
            for (int i = static_cast<int>(activeWs->tabs.size()) - 1; i >= 0; i--) {
                if (i != context_menu_tab_index_ && on_tab_close_) {
                    on_tab_close_(activeWs->tabs[i]->id);
                }
            }
            break;

        case CMD_MOVE_TO_NEW_WORKSPACE:
            // Create new workspace and move tab to it
            if (on_tab_move_) {
                Workspace* newWs = tab_manager_->CreateWorkspace("");
                if (newWs) {
                    on_tab_move_(tab_id, newWs->id);
                }
            }
            break;

        default:
            // Check if it's a move to workspace command
            if (cmd_id >= CMD_MOVE_TO_WORKSPACE_BASE && cmd_id < CMD_MOVE_TO_NEW_WORKSPACE) {
                int target_workspace_id = cmd_id - CMD_MOVE_TO_WORKSPACE_BASE;
                if (on_tab_move_) {
                    on_tab_move_(tab_id, target_workspace_id);
                }
            }
            break;
    }

    context_menu_tab_index_ = -1;
}

void SidebarWindow::ShowPanel(Panel panel) {
    current_panel_ = panel;

    // Hide all panels first
    if (bookmarks_panel_) bookmarks_panel_->Hide();
    if (history_panel_) history_panel_->Hide();
    if (downloads_panel_) downloads_panel_->Hide();
    if (settings_panel_) settings_panel_->Hide();

    // Show the selected panel
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;

    switch (panel) {
        case Panel::Tabs:
            // Tabs panel is drawn directly, no separate window
            break;
        case Panel::Bookmarks:
            if (bookmarks_panel_) {
                SetWindowPos(bookmarks_panel_->GetHWND(), nullptr,
                    panelWidth, 0,
                    clientRect.right - panelWidth, clientRect.bottom,
                    SWP_NOZORDER);
                bookmarks_panel_->Show();
            }
            break;
        case Panel::History:
            if (history_panel_) {
                SetWindowPos(history_panel_->GetHWND(), nullptr,
                    panelWidth, 0,
                    clientRect.right - panelWidth, clientRect.bottom,
                    SWP_NOZORDER);
                history_panel_->Show();
            }
            break;
        case Panel::Downloads:
            if (downloads_panel_) {
                SetWindowPos(downloads_panel_->GetHWND(), nullptr,
                    panelWidth, 0,
                    clientRect.right - panelWidth, clientRect.bottom,
                    SWP_NOZORDER);
                downloads_panel_->Show();
            }
            break;
        case Panel::Settings:
            if (settings_panel_) {
                SetWindowPos(settings_panel_->GetHWND(), nullptr,
                    panelWidth, 0,
                    clientRect.right - panelWidth, clientRect.bottom,
                    SWP_NOZORDER);
                settings_panel_->Show();
            }
            break;
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SidebarWindow::RefreshTabList() {
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ============================================================================
// Workspace Operations
// ============================================================================

void SidebarWindow::AddNewWorkspace() {
    if (!tab_manager_) return;

    // Create new workspace with auto-generated name
    std::string name = tab_manager_->GenerateUniqueWorkspaceName();
    Workspace* newWs = tab_manager_->CreateWorkspace(name);
    if (newWs) {
        tab_manager_->SetActiveWorkspace(newWs->id);
        InvalidateRect(hwnd_, nullptr, FALSE);

        // Notify callback
        if (on_new_workspace_) {
            on_new_workspace_();
        }
    }
}

void SidebarWindow::DeleteWorkspace(int workspace_id) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();

    // Don't delete if it's the last workspace
    if (workspaces.size() <= 1) {
        return;
    }

    // Find the workspace to check for pinned tabs
    Workspace* ws = nullptr;
    for (const auto& w : workspaces) {
        if (w->id == workspace_id) {
            ws = w.get();
            break;
        }
    }

    if (!ws) return;

    // Check for pinned tabs and show confirmation
    bool hasPinnedTabs = false;
    for (const auto& tab : ws->tabs) {
        if (tab->is_pinned) {
            hasPinnedTabs = true;
            break;
        }
    }

    if (hasPinnedTabs) {
        int result = MessageBoxW(hwnd_,
            L"This workspace has pinned tabs. Are you sure you want to delete it?",
            L"Delete Workspace",
            MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) {
            return;
        }
    }

    tab_manager_->DeleteWorkspace(workspace_id);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SidebarWindow::RenameWorkspace(int workspace_id) {
    if (!tab_manager_) return;

    // Find workspace index
    const auto& workspaces = tab_manager_->GetWorkspaces();
    for (size_t i = 0; i < workspaces.size(); i++) {
        if (workspaces[i]->id == workspace_id) {
            StartInlineRename(static_cast<int>(i));
            return;
        }
    }
}

void SidebarWindow::DuplicateWorkspace(int workspace_id) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();

    // Find the workspace to duplicate
    Workspace* sourceWs = nullptr;
    for (const auto& ws : workspaces) {
        if (ws->id == workspace_id) {
            sourceWs = ws.get();
            break;
        }
    }

    if (!sourceWs) return;

    // Create new workspace with copied name
    std::string newName = sourceWs->name + " (Copy)";
    Workspace* newWs = tab_manager_->CreateWorkspace(newName);
    if (newWs) {
        newWs->color = sourceWs->color;
        tab_manager_->SetActiveWorkspace(newWs->id);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SidebarWindow::ChangeWorkspaceColor(int workspace_id, int color_index) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();

    for (const auto& ws : workspaces) {
        if (ws->id == workspace_id) {
            ws->color = WorkspaceColors::ForIndex(color_index);
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
    }
}

void SidebarWindow::ShowWorkspaceContextMenu(int x, int y, int workspace_index) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspace_index < 0 || workspace_index >= static_cast<int>(workspaces.size())) return;

    context_menu_workspace_index_ = workspace_index;
    int workspace_id = workspaces[workspace_index]->id;

    HMENU menu = CreatePopupMenu();

    // Rename
    AppendMenuW(menu, MF_STRING, IDM_WORKSPACE_RENAME, L"Rename Space");

    // Duplicate
    AppendMenuW(menu, MF_STRING, IDM_WORKSPACE_DUPLICATE, L"Duplicate Space");

    // Separator
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Change Color submenu
    HMENU colorMenu = CreatePopupMenu();
    const wchar_t* colorNames[] = { L"Blue", L"Red", L"Green", L"Orange", L"Purple", L"Pink", L"Teal", L"Yellow" };
    for (int i = 0; i < 8; i++) {
        AppendMenuW(colorMenu, MF_STRING, IDM_WORKSPACE_COLOR_BASE + i, colorNames[i]);
    }
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)colorMenu, L"Change Color");

    // Separator
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Delete (disabled if only one workspace)
    UINT deleteFlags = MF_STRING;
    if (workspaces.size() <= 1) {
        deleteFlags |= MF_GRAYED;
    }
    AppendMenuW(menu, deleteFlags, IDM_WORKSPACE_DELETE, L"Delete Space");

    // Show context menu
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);

    DestroyMenu(colorMenu);
    DestroyMenu(menu);
}

void SidebarWindow::OnWorkspaceContextMenuCommand(int cmd_id) {
    if (!tab_manager_ || context_menu_workspace_index_ < 0) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (context_menu_workspace_index_ >= static_cast<int>(workspaces.size())) return;

    int workspace_id = workspaces[context_menu_workspace_index_]->id;

    if (cmd_id == IDM_WORKSPACE_RENAME) {
        RenameWorkspace(workspace_id);
    } else if (cmd_id == IDM_WORKSPACE_DUPLICATE) {
        DuplicateWorkspace(workspace_id);
    } else if (cmd_id == IDM_WORKSPACE_DELETE) {
        DeleteWorkspace(workspace_id);
    } else if (cmd_id >= IDM_WORKSPACE_COLOR_BASE && cmd_id < IDM_WORKSPACE_COLOR_BASE + 8) {
        ChangeWorkspaceColor(workspace_id, cmd_id - IDM_WORKSPACE_COLOR_BASE);
    }

    context_menu_workspace_index_ = -1;
}

// ============================================================================
// Inline Rename
// ============================================================================

void SidebarWindow::StartInlineRename(int workspace_index) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspace_index < 0 || workspace_index >= static_cast<int>(workspaces.size())) return;

    // End any existing rename
    if (rename_edit_) {
        EndInlineRename(false);
    }

    rename_workspace_index_ = workspace_index;

    // Get the rect for the workspace tab
    RECT tabRect = GetWorkspaceTabRect(workspace_index);

    // Adjust for text area only (skip color dot)
    int dotSize = 8;
    tabRect.left += 6 + dotSize + 4;
    tabRect.right -= 4;

    // Create edit control
    rename_edit_ = CreateWindowExW(
        0,
        L"EDIT",
        DesignSystem::Utf8ToWide(workspaces[workspace_index]->name).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        tabRect.left, tabRect.top + 4,
        tabRect.right - tabRect.left, tabRect.bottom - tabRect.top - 8,
        hwnd_,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (rename_edit_) {
        // Style the edit control
        SendMessage(rename_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
        SendMessage(rename_edit_, EM_SETSEL, 0, -1);  // Select all text
        SetFocus(rename_edit_);

        // Subclass to handle Enter/Escape
        SetWindowLongPtr(rename_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        rename_edit_original_proc_ = (WNDPROC)SetWindowLongPtr(rename_edit_, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(RenameEditProc));
    }
}

void SidebarWindow::EndInlineRename(bool save) {
    if (!rename_edit_ || rename_workspace_index_ < 0) return;

    if (save && tab_manager_) {
        const auto& workspaces = tab_manager_->GetWorkspaces();
        if (rename_workspace_index_ < static_cast<int>(workspaces.size())) {
            wchar_t buffer[256];
            GetWindowTextW(rename_edit_, buffer, 256);
            std::wstring wname(buffer);
            if (!wname.empty()) {
                workspaces[rename_workspace_index_]->name = DesignSystem::WideToUtf8(wname);
            }
        }
    }

    // Restore original wndproc and destroy
    if (rename_edit_original_proc_) {
        SetWindowLongPtr(rename_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(rename_edit_original_proc_));
        rename_edit_original_proc_ = nullptr;
    }

    DestroyWindow(rename_edit_);
    rename_edit_ = nullptr;
    rename_workspace_index_ = -1;

    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK SidebarWindow::RenameEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SidebarWindow* self = reinterpret_cast<SidebarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                if (self) self->EndInlineRename(true);
                return 0;
            } else if (wParam == VK_ESCAPE) {
                if (self) self->EndInlineRename(false);
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            // Save on focus loss
            if (self) self->EndInlineRename(true);
            return 0;
    }

    if (self && self->rename_edit_original_proc_) {
        return CallWindowProc(self->rename_edit_original_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Drag and Drop for Workspace Reordering
// ============================================================================

void SidebarWindow::StartWorkspaceDrag(int workspace_index, int x, int y) {
    if (!tab_manager_) return;

    const auto& workspaces = tab_manager_->GetWorkspaces();
    if (workspace_index < 0 || workspace_index >= static_cast<int>(workspaces.size())) return;

    is_dragging_workspace_ = true;
    drag_workspace_index_ = workspace_index;
    drag_start_x_ = x;
    drag_current_x_ = x;
    drag_insert_index_ = -1;

    SetCapture(hwnd_);
}

void SidebarWindow::UpdateWorkspaceDrag(int x, int y) {
    (void)y;  // Unused but kept for consistency

    if (!is_dragging_workspace_ || !tab_manager_) return;

    drag_current_x_ = x;

    // Check if we've moved enough to consider this a real drag
    int dragThreshold = 5;
    if (abs(drag_current_x_ - drag_start_x_) < dragThreshold) {
        drag_insert_index_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    // Calculate insert position based on current mouse X
    const auto& workspaces = tab_manager_->GetWorkspaces();
    int tabWidth = GetWorkspaceTabWidth();

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int panelWidth = panel_icon_size_ + panel_icon_margin_ * 2;
    int startX = panelWidth + DesignSystem::GetSpacingSM() - workspace_scroll_offset_;

    int newInsertIndex = -1;
    for (size_t i = 0; i <= workspaces.size(); i++) {
        int tabCenterX = startX + static_cast<int>(i) * tabWidth - tabWidth / 2;
        if (i == 0) tabCenterX = startX;
        if (drag_current_x_ < tabCenterX + tabWidth / 2) {
            newInsertIndex = static_cast<int>(i);
            break;
        }
    }

    if (newInsertIndex < 0) {
        newInsertIndex = static_cast<int>(workspaces.size());
    }

    // Don't show insert indicator at the dragged item's current position or adjacent
    if (newInsertIndex == drag_workspace_index_ || newInsertIndex == drag_workspace_index_ + 1) {
        newInsertIndex = -1;
    }

    if (newInsertIndex != drag_insert_index_) {
        drag_insert_index_ = newInsertIndex;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SidebarWindow::EndWorkspaceDrag() {
    if (!is_dragging_workspace_) return;

    ReleaseCapture();

    // Perform the reorder if we have a valid insert position
    if (drag_insert_index_ >= 0 && tab_manager_) {
        const auto& workspaces = tab_manager_->GetWorkspaces();
        if (drag_workspace_index_ >= 0 && drag_workspace_index_ < static_cast<int>(workspaces.size())) {
            int workspace_id = workspaces[drag_workspace_index_]->id;

            // Adjust insert index if moving forward
            int target_index = drag_insert_index_;
            if (drag_insert_index_ > drag_workspace_index_) {
                target_index--;  // Account for removal of source item
            }

            tab_manager_->ReorderWorkspace(workspace_id, target_index);
        }
    }

    is_dragging_workspace_ = false;
    drag_workspace_index_ = -1;
    drag_insert_index_ = -1;

    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ============================================================================
// Favicon Cache Management
// ============================================================================

void SidebarWindow::UpdateFaviconCache(int tab_id, const std::vector<unsigned char>& png_data) {
    if (png_data.empty()) return;

    // Create IStream from PNG data
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, png_data.size());
    if (!hMem) return;

    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        return;
    }

    memcpy(pMem, png_data.data(), png_data.size());
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
        // Create bitmap from stream
        auto bitmap = std::make_unique<Gdiplus::Bitmap>(pStream);
        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
            favicon_cache_[tab_id] = std::move(bitmap);
        }
        pStream->Release();
    } else {
        GlobalFree(hMem);
    }
}

void SidebarWindow::ClearFaviconCache() {
    favicon_cache_.clear();
}

Gdiplus::Bitmap* SidebarWindow::GetCachedFavicon(int tab_id) {
    auto it = favicon_cache_.find(tab_id);
    if (it != favicon_cache_.end()) {
        return it->second.get();
    }
    return nullptr;
}

#endif  // PLATFORM_WIN
