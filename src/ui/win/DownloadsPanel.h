#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct DownloadItem;
enum class DownloadState;

// Downloads panel for the sidebar
// Shows download progress and completed downloads
class DownloadsPanel {
public:
    DownloadsPanel();
    ~DownloadsPanel();

    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHWND() const { return hwnd_; }

    void Refresh();

    // Callback when user wants to open a downloaded file
    using OpenFileCallback = std::function<void(const std::string& path)>;
    void SetOpenFileCallback(OpenFileCallback cb) { on_open_file_ = std::move(cb); }

    // Callback to retry a download (navigates to URL in the active browser)
    using RetryDownloadCallback = std::function<void(const std::string& url)>;
    void SetRetryDownloadCallback(RetryDownloadCallback cb) { on_retry_download_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    void OnTimer();

    void DrawDownloadRow(HDC hdc, const DownloadItem& item, int y, bool hovered);
    void DrawProgressBar(HDC hdc, int x, int y, int width, int height, int percent);
    int HitTestRow(int x, int y);
    void ShowContextMenu(int x, int y, int index);

    std::wstring FormatFileSize(int64_t bytes);
    std::wstring GetStateText(DownloadState state);
    COLORREF GetStateColor(DownloadState state);

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // State
    int scroll_offset_ = 0;
    int content_height_ = 0;
    int hovered_row_ = -1;
    bool tracking_mouse_ = false;

    // Cached downloads
    std::vector<DownloadItem> downloads_;

    // Layout
    static constexpr int kHeaderHeight = 40;
    static constexpr int kRowHeight = 60;

    // Timer for progress updates
    static constexpr UINT_PTR kTimerId = 1;

    OpenFileCallback on_open_file_;
    RetryDownloadCallback on_retry_download_;

    HFONT font_normal_ = nullptr;
    HFONT font_small_ = nullptr;
    HFONT font_bold_ = nullptr;

    static constexpr wchar_t kClassName[] = L"OrbFoxDownloadsPanel";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
