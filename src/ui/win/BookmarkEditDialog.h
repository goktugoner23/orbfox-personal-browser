#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

// Mode for the bookmark dialog
enum class BookmarkDialogMode {
    Add,
    Edit
};

// Modal dialog for adding or editing bookmarks
// Dark-themed to match the OrbFox design system
class BookmarkEditDialog {
public:
    BookmarkEditDialog();
    ~BookmarkEditDialog();

    // Result of the dialog
    enum class Result {
        Save,
        Cancel,
        Delete
    };

    // Show dialog for adding a new bookmark
    // Returns true if the bookmark was saved
    Result ShowAdd(HWND parent, const std::string& url, const std::string& title);

    // Show dialog for editing an existing bookmark
    // Returns result indicating user action
    Result ShowEdit(HWND parent, int64_t bookmark_id);

    // Callbacks (called when dialog completes successfully)
    using SaveCallback = std::function<void()>;
    void SetSaveCallback(SaveCallback cb) { on_save_ = std::move(cb); }

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Create the dialog window and controls
    bool Create(HWND parent);
    void Destroy();

    // Message handling
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnPaint();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnClose();

    // Populate the folder dropdown
    void PopulateFolderPicker();

    // Handle save action
    void OnSave();
    void OnRemove();

    // Show new folder input dialog
    void ShowNewFolderDialog();

    // Data
    BookmarkDialogMode mode_ = BookmarkDialogMode::Add;
    int64_t edit_bookmark_id_ = 0;
    std::string initial_url_;
    std::string initial_title_;
    std::string initial_folder_;
    std::string initial_description_;
    Result dialog_result_ = Result::Cancel;

    // Window handles
    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;

    // Controls
    HWND url_label_ = nullptr;
    HWND url_edit_ = nullptr;
    HWND title_label_ = nullptr;
    HWND title_edit_ = nullptr;
    HWND desc_label_ = nullptr;
    HWND desc_edit_ = nullptr;
    HWND folder_label_ = nullptr;
    HWND folder_combo_ = nullptr;
    HWND save_button_ = nullptr;
    HWND cancel_button_ = nullptr;
    HWND remove_button_ = nullptr;  // Only visible in edit mode

    // Original window procedures for subclassing
    WNDPROC original_edit_proc_ = nullptr;
    WNDPROC original_combo_proc_ = nullptr;

    // Cached folder list
    std::vector<std::string> folders_;

    // Fonts
    HFONT font_normal_ = nullptr;
    HFONT font_label_ = nullptr;

    // Callback
    SaveCallback on_save_;

    // Layout constants
    static constexpr int kDialogWidth = 380;
    static constexpr int kDialogHeight = 340;
    static constexpr int kPadding = 16;
    static constexpr int kLabelHeight = 18;
    static constexpr int kFieldHeight = 28;
    static constexpr int kDescHeight = 60;
    static constexpr int kButtonWidth = 80;
    static constexpr int kButtonHeight = 28;
    static constexpr int kSpacing = 8;

    // Control IDs
    static constexpr int ID_URL_EDIT = 1001;
    static constexpr int ID_TITLE_EDIT = 1002;
    static constexpr int ID_DESC_EDIT = 1003;
    static constexpr int ID_FOLDER_COMBO = 1004;
    static constexpr int ID_SAVE_BUTTON = 1005;
    static constexpr int ID_CANCEL_BUTTON = 1006;
    static constexpr int ID_REMOVE_BUTTON = 1007;

    // Special folder indices
    static constexpr int kNoFolderIndex = 0;
    static constexpr int kNewFolderIndex = 1;

    static constexpr wchar_t kClassName[] = L"OrbFoxBookmarkEditDialog";
    static bool class_registered_;
};

#endif  // PLATFORM_WIN
