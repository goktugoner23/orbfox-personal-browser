#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

class BookmarkStorage;
struct Bookmark;

// Bookmarks panel for the sidebar
// Displays bookmarks organized by folders with expand/collapse
class BookmarksPanel {
public:
    BookmarksPanel();
    ~BookmarksPanel();

    // Create as child of sidebar
    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHWND() const { return hwnd_; }

    // Refresh the bookmark list
    void Refresh();

    // Callbacks
    using OpenUrlCallback = std::function<void(const std::string& url, bool background)>;
    using EditBookmarkCallback = std::function<void(int64_t bookmark_id)>;

    void SetOpenUrlCallback(OpenUrlCallback cb) { on_open_url_ = std::move(cb); }
    void SetEditBookmarkCallback(EditBookmarkCallback cb) { on_edit_bookmark_ = std::move(cb); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseWheel(int delta);
    void OnMouseLeave();

    // Drawing
    void DrawFolderRow(HDC hdc, const std::string& folder, int y, bool expanded, bool hovered);
    void DrawBookmarkRow(HDC hdc, const Bookmark& bookmark, int y, bool hovered);

    // Hit testing
    struct HitResult {
        enum Type { None, Folder, Bookmark, FolderExpand };
        Type type = None;
        int index = -1;
        std::string folder_name;
    };
    HitResult HitTest(int x, int y);

    // Show context menu
    void ShowContextMenu(int x, int y, const HitResult& hit);

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // Scroll state
    int scroll_offset_ = 0;
    int content_height_ = 0;

    // Hover state
    HitResult hovered_;
    bool tracking_mouse_ = false;

    // Expanded folders
    std::vector<std::string> expanded_folders_;

    // Cached data
    std::vector<std::string> folders_;
    std::vector<Bookmark> bookmarks_;

    // Layout
    static constexpr int kRowHeight = 32;
    static constexpr int kIndent = 20;

    // Callbacks
    OpenUrlCallback on_open_url_;
    EditBookmarkCallback on_edit_bookmark_;

    // Fonts
    HFONT font_normal_ = nullptr;
    HFONT font_bold_ = nullptr;

    static constexpr wchar_t kClassName[] = L"OrbFoxBookmarksPanel";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
