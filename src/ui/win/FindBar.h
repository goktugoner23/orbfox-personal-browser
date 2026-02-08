#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <functional>

// Find bar for "Find in Page" functionality
// Appears at the top of the browser area
class FindBar {
public:
    FindBar();
    ~FindBar();

    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHWND() const { return hwnd_; }

    void SetMatchCount(int count, int current);
    void Focus();

    // Callbacks
    using FindCallback = std::function<void(const std::string& text, bool forward, bool match_case)>;
    using CloseCallback = std::function<void()>;

    void SetFindCallback(FindCallback cb) { on_find_ = std::move(cb); }
    void SetCloseCallback(CloseCallback cb) { on_close_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnFindTextChanged();

    void UpdateLayout();

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND search_edit_ = nullptr;
    HWND prev_button_ = nullptr;
    HWND next_button_ = nullptr;
    HWND close_button_ = nullptr;
    HWND match_label_ = nullptr;

    WNDPROC original_edit_proc_ = nullptr;

    int match_count_ = 0;
    int current_match_ = 0;
    std::wstring search_text_;

    FindCallback on_find_;
    CloseCallback on_close_;

    HFONT font_ = nullptr;

    static constexpr int kBarHeight = 36;
    static constexpr int ID_SEARCH = 1001;
    static constexpr int ID_PREV = 1002;
    static constexpr int ID_NEXT = 1003;
    static constexpr int ID_CLOSE = 1004;

    static constexpr wchar_t kClassName[] = L"OrbFoxFindBar";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
