#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <functional>

// Settings panel for the sidebar
// Displays browser settings organized into sections:
// - General: New tab URL, Restore session
// - Privacy: Tracking protection, Clear browsing data
// - Downloads: Download location, Ask before downloading
// - About: Version info, Help/Feedback links
class SettingsPanel {
public:
    SettingsPanel();
    ~SettingsPanel();

    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHWND() const { return hwnd_; }

    void Refresh();

    // Callback for opening URLs (for help/feedback links)
    using OpenUrlCallback = std::function<void(const std::string& url, bool background)>;
    void SetOpenUrlCallback(OpenUrlCallback cb) { on_open_url_ = std::move(cb); }

    // Callback for clearing browsing data
    using ClearDataCallback = std::function<void()>;
    void SetClearDataCallback(ClearDataCallback cb) { on_clear_data_ = std::move(cb); }

    // Callback for browsing for download folder
    using BrowseFolderCallback = std::function<std::string()>;
    void SetBrowseFolderCallback(BrowseFolderCallback cb) { on_browse_folder_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseWheel(int delta);
    void OnMouseLeave();
    void OnCommand(WPARAM wParam, LPARAM lParam);

    void LoadSettings();
    void SaveSettings();
    void UpdateLayout();

    // Drawing helpers
    void DrawSectionHeader(HDC hdc, int& y, const std::wstring& title);
    void DrawToggleRow(HDC hdc, int& y, const std::wstring& label, bool value, int id);
    void DrawButtonRow(HDC hdc, int& y, const std::wstring& label, const std::wstring& buttonText, int id);
    void DrawLinkRow(HDC hdc, int& y, const std::wstring& text, int id);
    void DrawInputRow(HDC hdc, int& y, const std::wstring& label);

    // Hit testing for toggle switches and buttons
    struct HitTestResult {
        int type;  // 0 = none, 1 = toggle, 2 = button, 3 = link
        int id;
    };
    HitTestResult HitTest(int x, int y);

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // Edit controls
    HWND new_tab_url_edit_ = nullptr;
    HWND download_path_edit_ = nullptr;

    WNDPROC original_edit_proc_ = nullptr;

    // State
    int scroll_offset_ = 0;
    int content_height_ = 0;
    int hovered_item_id_ = -1;
    bool tracking_mouse_ = false;

    // Layout constants
    static constexpr int kHeaderHeight = 40;
    static constexpr int kSectionHeaderHeight = 32;
    static constexpr int kRowHeight = 44;
    static constexpr int kInputRowHeight = 64;
    static constexpr int kPadding = 16;
    static constexpr int kToggleWidth = 44;
    static constexpr int kToggleHeight = 24;

    // Control IDs
    static constexpr int ID_NEW_TAB_URL_EDIT = 1001;
    static constexpr int ID_DOWNLOAD_PATH_EDIT = 1002;
    static constexpr int ID_RESTORE_SESSION_TOGGLE = 2001;
    static constexpr int ID_TRACKING_PROTECTION_TOGGLE = 2002;
    static constexpr int ID_ASK_DOWNLOAD_TOGGLE = 2003;
    static constexpr int ID_CLEAR_DATA_BUTTON = 3001;
    static constexpr int ID_BROWSE_FOLDER_BUTTON = 3002;
    static constexpr int ID_HELP_LINK = 4001;
    static constexpr int ID_FEEDBACK_LINK = 4002;

    // Cached settings
    bool restore_session_ = true;
    bool tracking_protection_ = true;
    bool ask_before_download_ = true;

    OpenUrlCallback on_open_url_;
    ClearDataCallback on_clear_data_;
    BrowseFolderCallback on_browse_folder_;

    HFONT font_normal_ = nullptr;
    HFONT font_bold_ = nullptr;
    HFONT font_small_ = nullptr;

    static constexpr wchar_t kClassName[] = L"OrbFoxSettingsPanel";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
