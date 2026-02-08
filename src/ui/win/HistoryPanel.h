#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct HistoryEntry;

// History panel for the sidebar
// Displays browsing history with search and clear functionality
class HistoryPanel {
public:
    HistoryPanel();
    ~HistoryPanel();

    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHWND() const { return hwnd_; }

    void Refresh();

    // Callbacks
    using OpenUrlCallback = std::function<void(const std::string& url, bool background)>;
    void SetOpenUrlCallback(OpenUrlCallback cb) { on_open_url_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseWheel(int delta);
    void OnMouseLeave();
    void OnSearchChanged();

    void DrawHistoryRow(HDC hdc, const HistoryEntry& entry, int y, bool hovered);
    int HitTestRow(int x, int y);
    void ShowContextMenu(int x, int y, int index);
    void UpdateLayout();

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND search_edit_ = nullptr;
    HWND clear_button_ = nullptr;

    WNDPROC original_search_proc_ = nullptr;

    // State
    int scroll_offset_ = 0;
    int content_height_ = 0;
    int hovered_row_ = -1;
    bool tracking_mouse_ = false;
    std::wstring search_text_;

    // Cached/filtered entries
    std::vector<HistoryEntry> entries_;
    std::vector<HistoryEntry> filtered_entries_;

    // Layout
    static constexpr int kHeaderHeight = 80;
    static constexpr int kRowHeight = 44;
    static constexpr int kSearchHeight = 28;

    OpenUrlCallback on_open_url_;

    HFONT font_normal_ = nullptr;
    HFONT font_small_ = nullptr;

    static constexpr wchar_t kClassName[] = L"OrbFoxHistoryPanel";
    static bool class_registered_;

    static constexpr int ID_SEARCH = 1001;
    static constexpr int ID_CLEAR = 1002;
};

#endif  // PLATFORM_WIN
