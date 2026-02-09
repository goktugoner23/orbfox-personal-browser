#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

class TabManager;
class BookmarksPanel;
class HistoryPanel;
class DownloadsPanel;
class SettingsPanel;

// Sidebar window for OrbFox on Windows
// Contains panel icons, workspace tabs, and tab list
class SidebarWindow {
public:
    enum class Panel {
        Tabs,
        Bookmarks,
        History,
        Downloads,
        Settings
    };

    explicit SidebarWindow(TabManager* tab_manager);
    ~SidebarWindow();

    // Create the sidebar as a child of the main window
    bool Create(HWND parent);

    // Get window handle
    HWND GetHWND() const { return hwnd_; }

    // Show specific panel
    void ShowPanel(Panel panel);

    // Refresh the tab list display
    void RefreshTabList();

    // Callbacks
    using TabSelectedCallback = std::function<void(int tab_id)>;
    using TabCloseCallback = std::function<void(int tab_id)>;
    using TabDuplicateCallback = std::function<void(int tab_id)>;
    using TabReloadCallback = std::function<void(int tab_id)>;
    using TabMoveCallback = std::function<void(int tab_id, int workspace_id)>;
    using NewTabCallback = std::function<void()>;
    using WorkspaceSelectedCallback = std::function<void(int workspace_id)>;
    using NewWorkspaceCallback = std::function<void()>;

    void SetTabSelectedCallback(TabSelectedCallback callback) { on_tab_selected_ = std::move(callback); }
    void SetTabCloseCallback(TabCloseCallback callback) { on_tab_close_ = std::move(callback); }
    void SetTabDuplicateCallback(TabDuplicateCallback callback) { on_tab_duplicate_ = std::move(callback); }
    void SetTabReloadCallback(TabReloadCallback callback) { on_tab_reload_ = std::move(callback); }
    void SetTabMoveCallback(TabMoveCallback callback) { on_tab_move_ = std::move(callback); }
    void SetNewTabCallback(NewTabCallback callback) { on_new_tab_ = std::move(callback); }
    void SetWorkspaceSelectedCallback(WorkspaceSelectedCallback callback) { on_workspace_selected_ = std::move(callback); }
    void SetNewWorkspaceCallback(NewWorkspaceCallback callback) { on_new_workspace_ = std::move(callback); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Register window class
    static bool RegisterWindowClass(HINSTANCE hInstance);

    // Message handlers
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnDestroy();
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnLButtonDblClk(int x, int y);
    void OnMButtonUp(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseWheel(int x, int y, int delta);
    void OnMouseLeave();
    void OnTimer(UINT_PTR timer_id);
    void OnContextMenuCommand(int cmd_id);
    void OnWorkspaceContextMenuCommand(int cmd_id);

    // Drawing
    void DrawPanelIcons(HDC hdc);
    void DrawWorkspaceTabs(HDC hdc);
    void DrawAddWorkspaceButton(HDC hdc);
    void DrawTabList(HDC hdc);
    void DrawNewTabButton(HDC hdc);

    // Hit testing
    int HitTestPanelIcon(int x, int y);
    int HitTestWorkspaceTab(int x, int y);
    bool HitTestWorkspaceCloseButton(int x, int y, int workspace_index);
    bool HitTestAddWorkspaceButton(int x, int y);
    int HitTestTabRow(int x, int y);
    bool HitTestNewTabButton(int x, int y);
    bool HitTestTabCloseButton(int x, int y, int tab_index);
    RECT GetWorkspaceTabRect(int workspace_index);
    RECT GetWorkspaceCloseButtonRect(int workspace_index);
    RECT GetAddWorkspaceButtonRect();
    RECT GetTabRowRect(int tab_index);
    RECT GetTabCloseButtonRect(int tab_index);
    int GetWorkspaceTabWidth();

    // Context menu
    void ShowTabContextMenu(int x, int y, int tab_index);
    void ShowWorkspaceContextMenu(int x, int y, int workspace_index);

    // Workspace operations
    void AddNewWorkspace();
    void DeleteWorkspace(int workspace_id);
    void RenameWorkspace(int workspace_id);
    void DuplicateWorkspace(int workspace_id);
    void ChangeWorkspaceColor(int workspace_id, int color_index);

    // Inline rename
    void StartInlineRename(int workspace_index);
    void EndInlineRename(bool save);
    static LRESULT CALLBACK RenameEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Drag and drop for workspace reordering
    void StartWorkspaceDrag(int workspace_index, int x, int y);
    void UpdateWorkspaceDrag(int x, int y);
    void EndWorkspaceDrag();

    // Favicon management
    void UpdateFaviconCache(int tab_id, const std::vector<unsigned char>& png_data);
    void ClearFaviconCache();
    Gdiplus::Bitmap* GetCachedFavicon(int tab_id);

    // Window handle
    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // Tab management
    TabManager* tab_manager_ = nullptr;

    // Current state
    Panel current_panel_ = Panel::Tabs;
    int hovered_panel_icon_ = -1;
    int hovered_workspace_ = -1;
    int hovered_workspace_close_ = -1;  // Index of workspace whose close button is hovered
    int hovered_tab_ = -1;
    bool hovered_close_button_ = false;
    bool hovered_new_tab_ = false;
    bool hovered_add_workspace_ = false;
    bool tracking_mouse_ = false;
    int context_menu_tab_index_ = -1;  // Tab index for context menu actions
    int context_menu_workspace_index_ = -1;  // Workspace index for context menu actions

    // Workspace horizontal scroll
    int workspace_scroll_offset_ = 0;
    int workspace_total_width_ = 0;

    // Inline rename state
    HWND rename_edit_ = nullptr;
    int rename_workspace_index_ = -1;
    WNDPROC rename_edit_original_proc_ = nullptr;

    // Drag state for workspace reordering
    bool is_dragging_workspace_ = false;
    int drag_workspace_index_ = -1;
    int drag_start_x_ = 0;
    int drag_current_x_ = 0;
    int drag_insert_index_ = -1;

    // Loading animation
    static constexpr UINT_PTR kLoadingTimerId = 1;
    int loading_animation_frame_ = 0;

    // Layout
    int panel_icon_size_ = 32;
    int panel_icon_margin_ = 8;
    int workspace_tab_height_ = 32;
    int workspace_tab_min_width_ = 60;
    int workspace_tab_max_width_ = 120;
    int add_workspace_btn_size_ = 28;
    int tab_row_height_ = 36;

    // Callbacks
    TabSelectedCallback on_tab_selected_;
    TabCloseCallback on_tab_close_;
    TabDuplicateCallback on_tab_duplicate_;
    TabReloadCallback on_tab_reload_;
    TabMoveCallback on_tab_move_;
    NewTabCallback on_new_tab_;
    WorkspaceSelectedCallback on_workspace_selected_;
    NewWorkspaceCallback on_new_workspace_;

    // Panels
    std::unique_ptr<BookmarksPanel> bookmarks_panel_;
    std::unique_ptr<HistoryPanel> history_panel_;
    std::unique_ptr<DownloadsPanel> downloads_panel_;
    std::unique_ptr<SettingsPanel> settings_panel_;

    // Open URL callback (passed to panels)
    using OpenUrlCallback = std::function<void(const std::string& url, bool background)>;
    OpenUrlCallback on_open_url_;

public:
    void SetOpenUrlCallback(OpenUrlCallback cb) { on_open_url_ = std::move(cb); }

private:
    // Fonts
    HFONT font_normal_ = nullptr;
    HFONT font_bold_ = nullptr;
    HFONT font_small_ = nullptr;
    HFONT font_icon_ = nullptr;  // For emoji/icon rendering

    // Favicon cache (tab_id -> bitmap)
    std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> favicon_cache_;

    // GDI+ token
    ULONG_PTR gdiplus_token_ = 0;

    // Context menu IDs for workspace
    static constexpr int IDM_WORKSPACE_RENAME = 2001;
    static constexpr int IDM_WORKSPACE_DUPLICATE = 2002;
    static constexpr int IDM_WORKSPACE_DELETE = 2003;
    static constexpr int IDM_WORKSPACE_COLOR_BASE = 2100;  // 2100-2107 for 8 colors

    // Window class name
    static constexpr wchar_t kWindowClassName[] = L"OrbFoxSidebar";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
