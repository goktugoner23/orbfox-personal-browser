#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

// Single autocomplete suggestion
struct AutocompleteSuggestion {
    std::string url;
    std::string title;
    bool is_bookmarked = false;
    // Note: favicon support can be added later when Windows favicon caching is implemented
};

// Autocomplete dropdown for URL bar
// Shows suggestions from bookmarks and history based on typed query
class AutocompleteDropdown {
public:
    AutocompleteDropdown();
    ~AutocompleteDropdown();

    // Create as child of parent window (typically the main window)
    bool Create(HWND parent);

    // Get window handle
    HWND GetHWND() const { return hwnd_; }

    // Position the dropdown below the URL bar
    void SetPosition(int x, int y, int width);

    // Update suggestions based on query
    void UpdateSuggestionsForQuery(const std::string& query);

    // Show/hide
    void Show();
    void Hide();
    bool IsVisible() const;

    // Keyboard navigation
    void MoveSelectionUp();
    void MoveSelectionDown();
    std::string GetSelectedURL() const;
    bool HasSelection() const;

    // Get suggestions
    const std::vector<AutocompleteSuggestion>& GetSuggestions() const { return suggestions_; }

    // Callbacks
    using SelectCallback = std::function<void(const std::string& url)>;
    using DismissCallback = std::function<void()>;

    void SetSelectCallback(SelectCallback cb) { on_select_ = std::move(cb); }
    void SetDismissCallback(DismissCallback cb) { on_dismiss_ = std::move(cb); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnMouseWheel(int delta);
    void OnMouseLeave();

    // Drawing
    void DrawSuggestionRow(HDC hdc, const AutocompleteSuggestion& suggestion,
                           int y, bool selected, bool hovered);

    // Helper to extract domain from URL
    std::string GetDomainFromURL(const std::string& url);

    // Update layout/resize based on suggestion count
    void UpdateLayout();

    // Scroll selected row into view
    void ScrollToSelection();

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // Position (set by SetPosition)
    int pos_x_ = 0;
    int pos_y_ = 0;
    int width_ = 400;

    // Suggestions
    std::vector<AutocompleteSuggestion> suggestions_;

    // Selection state
    int selected_index_ = -1;  // -1 means no selection
    int hovered_index_ = -1;

    // Scroll state
    int scroll_offset_ = 0;

    // Mouse tracking
    bool tracking_mouse_ = false;

    // Layout constants
    static constexpr int kRowHeight = 44;
    static constexpr int kMaxVisibleRows = 8;
    static constexpr int kPadding = 8;
    static constexpr int kIconSize = 16;

    // Callbacks
    SelectCallback on_select_;
    DismissCallback on_dismiss_;

    // Font
    HFONT font_title_ = nullptr;
    HFONT font_url_ = nullptr;

    // Window class
    static constexpr wchar_t kClassName[] = L"OrbFoxAutocompleteDropdown";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
