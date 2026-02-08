#ifdef PLATFORM_WIN

#include "BookmarkEditDialog.h"
#include "DesignSystem.h"
#include "bookmark_storage.h"

#include <windowsx.h>
#include <algorithm>

bool BookmarkEditDialog::class_registered_ = false;

// External function to get bookmark storage
extern BookmarkStorage* GetBookmarkStorage();

BookmarkEditDialog::BookmarkEditDialog() = default;

BookmarkEditDialog::~BookmarkEditDialog() {
    Destroy();
}

BookmarkEditDialog::Result BookmarkEditDialog::ShowAdd(HWND parent, const std::string& url, const std::string& title) {
    mode_ = BookmarkDialogMode::Add;
    initial_url_ = url;
    initial_title_ = title;
    initial_folder_ = "";
    initial_description_ = "";
    edit_bookmark_id_ = 0;
    dialog_result_ = Result::Cancel;

    if (!Create(parent)) {
        return Result::Cancel;
    }

    // Set initial values
    SetWindowTextW(url_edit_, DesignSystem::Utf8ToWide(initial_url_).c_str());
    SetWindowTextW(title_edit_, DesignSystem::Utf8ToWide(initial_title_).c_str());
    SetWindowTextW(desc_edit_, L"");
    PopulateFolderPicker();
    SendMessage(folder_combo_, CB_SETCURSEL, kNoFolderIndex, 0);

    // Hide remove button in add mode
    ShowWindow(remove_button_, SW_HIDE);

    // Set button text
    SetWindowTextW(save_button_, L"Add");

    // Show dialog centered over parent
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - kDialogWidth) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - kDialogHeight) / 2;
    SetWindowPos(hwnd_, HWND_TOP, x, y, kDialogWidth, kDialogHeight, SWP_SHOWWINDOW);

    // Focus URL field
    SetFocus(url_edit_);
    SendMessage(url_edit_, EM_SETSEL, 0, -1);

    // Run modal message loop
    EnableWindow(parent, FALSE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hwnd_)) break;

        // Check for dialog-level key handling
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                dialog_result_ = Result::Cancel;
                DestroyWindow(hwnd_);
                break;
            } else if (msg.wParam == VK_RETURN) {
                // Enter key = save
                OnSave();
                break;
            }
        }

        if (!IsDialogMessage(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    Destroy();

    return dialog_result_;
}

BookmarkEditDialog::Result BookmarkEditDialog::ShowEdit(HWND parent, int64_t bookmark_id) {
    mode_ = BookmarkDialogMode::Edit;
    edit_bookmark_id_ = bookmark_id;
    dialog_result_ = Result::Cancel;

    // Get bookmark data
    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) return Result::Cancel;

    auto allBookmarks = storage->GetAllBookmarks();
    bool found = false;
    for (const auto& bm : allBookmarks) {
        if (bm.id == bookmark_id) {
            initial_url_ = bm.url;
            initial_title_ = bm.title;
            initial_folder_ = bm.folder;
            initial_description_ = "";  // Description not currently stored
            found = true;
            break;
        }
    }

    if (!found) return Result::Cancel;

    if (!Create(parent)) {
        return Result::Cancel;
    }

    // Set initial values
    SetWindowTextW(url_edit_, DesignSystem::Utf8ToWide(initial_url_).c_str());
    SetWindowTextW(title_edit_, DesignSystem::Utf8ToWide(initial_title_).c_str());
    SetWindowTextW(desc_edit_, DesignSystem::Utf8ToWide(initial_description_).c_str());
    PopulateFolderPicker();

    // Select the current folder
    if (initial_folder_.empty()) {
        SendMessage(folder_combo_, CB_SETCURSEL, kNoFolderIndex, 0);
    } else {
        // Find folder in list (skip "No Folder" and "New Folder..." items)
        int folderIndex = kNoFolderIndex;
        for (size_t i = 0; i < folders_.size(); i++) {
            if (folders_[i] == initial_folder_) {
                folderIndex = static_cast<int>(i) + 3;  // Skip "No Folder", "New Folder...", separator
                break;
            }
        }
        SendMessage(folder_combo_, CB_SETCURSEL, folderIndex, 0);
    }

    // Show remove button in edit mode
    ShowWindow(remove_button_, SW_SHOW);

    // Set button text
    SetWindowTextW(save_button_, L"Save");

    // Show dialog centered over parent
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - kDialogWidth) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - kDialogHeight) / 2;
    SetWindowPos(hwnd_, HWND_TOP, x, y, kDialogWidth, kDialogHeight, SWP_SHOWWINDOW);

    // Focus title field in edit mode
    SetFocus(title_edit_);
    SendMessage(title_edit_, EM_SETSEL, 0, -1);

    // Run modal message loop
    EnableWindow(parent, FALSE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hwnd_)) break;

        // Check for dialog-level key handling
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                dialog_result_ = Result::Cancel;
                DestroyWindow(hwnd_);
                break;
            } else if (msg.wParam == VK_RETURN) {
                // Enter key = save
                OnSave();
                break;
            }
        }

        if (!IsDialogMessage(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    Destroy();

    return dialog_result_;
}

bool BookmarkEditDialog::Create(HWND parent) {
    parent_ = parent;
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
        wcex.lpfnWndProc = BookmarkEditDialog::WndProc;
        wcex.cbWndExtra = sizeof(BookmarkEditDialog*);
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = kClassName;

        if (!RegisterClassExW(&wcex)) return false;
        class_registered_ = true;
    }

    // Create popup window with border
    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        kClassName,
        mode_ == BookmarkDialogMode::Add ? L"Add Bookmark" : L"Edit Bookmark",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, kDialogWidth, kDialogHeight,
        parent, nullptr, hInstance, this
    );

    return hwnd_ != nullptr;
}

void BookmarkEditDialog::Destroy() {
    if (font_normal_) {
        DeleteObject(font_normal_);
        font_normal_ = nullptr;
    }
    if (font_label_) {
        DeleteObject(font_label_);
        font_label_ = nullptr;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK BookmarkEditDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BookmarkEditDialog* dialog = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        dialog = reinterpret_cast<BookmarkEditDialog*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
        dialog->hwnd_ = hwnd;
    } else {
        dialog = reinterpret_cast<BookmarkEditDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (dialog) return dialog->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BookmarkEditDialog::EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BookmarkEditDialog* dialog = reinterpret_cast<BookmarkEditDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            // Allow tab navigation
            HWND next = GetNextDlgTabItem(dialog->hwnd_, hwnd, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            if (next) {
                SetFocus(next);
                return 0;
            }
        }
    }

    if (dialog && dialog->original_edit_proc_) {
        return CallWindowProc(dialog->original_edit_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BookmarkEditDialog::ComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BookmarkEditDialog* dialog = reinterpret_cast<BookmarkEditDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (dialog && dialog->original_combo_proc_) {
        return CallWindowProc(dialog->original_combo_proc_, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT BookmarkEditDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;
        case WM_CLOSE:
            OnClose();
            return 0;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
            SetBkColor(hdc, DesignSystem::GetSurfaceColor());
            static HBRUSH editBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
            return (LRESULT)editBrush;
        }
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DesignSystem::GetTextPrimaryColor());
            SetBkColor(hdc, DesignSystem::GetSurfaceColor());
            static HBRUSH listBrush = CreateSolidBrush(DesignSystem::GetSurfaceColor());
            return (LRESULT)listBrush;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void BookmarkEditDialog::OnCreate() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    font_normal_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());
    font_label_ = DesignSystem::CreateFont(DesignSystem::GetFontSizeSmall());

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    int width = clientRect.right;
    int y = kPadding;

    // URL Label
    url_label_ = CreateWindowExW(
        0, L"STATIC", L"Address",
        WS_CHILD | WS_VISIBLE,
        kPadding, y, 60, kLabelHeight,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessage(url_label_, WM_SETFONT, (WPARAM)font_label_, TRUE);
    y += kLabelHeight + 4;

    // URL Edit
    url_edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        kPadding, y, width - kPadding * 2, kFieldHeight,
        hwnd_, (HMENU)ID_URL_EDIT, hInstance, nullptr
    );
    SendMessage(url_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
    SetWindowLongPtr(url_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    original_edit_proc_ = (WNDPROC)SetWindowLongPtr(url_edit_, GWLP_WNDPROC,
                                                      reinterpret_cast<LONG_PTR>(EditProc));
    y += kFieldHeight + kSpacing;

    // Title Label
    title_label_ = CreateWindowExW(
        0, L"STATIC", L"Nickname",
        WS_CHILD | WS_VISIBLE,
        kPadding, y, 80, kLabelHeight,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessage(title_label_, WM_SETFONT, (WPARAM)font_label_, TRUE);
    y += kLabelHeight + 4;

    // Title Edit
    title_edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        kPadding, y, width - kPadding * 2, kFieldHeight,
        hwnd_, (HMENU)ID_TITLE_EDIT, hInstance, nullptr
    );
    SendMessage(title_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
    SetWindowLongPtr(title_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    y += kFieldHeight + kSpacing;

    // Description Label
    desc_label_ = CreateWindowExW(
        0, L"STATIC", L"Description",
        WS_CHILD | WS_VISIBLE,
        kPadding, y, 80, kLabelHeight,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessage(desc_label_, WM_SETFONT, (WPARAM)font_label_, TRUE);
    y += kLabelHeight + 4;

    // Description Edit (multiline)
    desc_edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        kPadding, y, width - kPadding * 2, kDescHeight,
        hwnd_, (HMENU)ID_DESC_EDIT, hInstance, nullptr
    );
    SendMessage(desc_edit_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
    SetWindowLongPtr(desc_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    y += kDescHeight + kSpacing;

    // Folder Label
    folder_label_ = CreateWindowExW(
        0, L"STATIC", L"Folder",
        WS_CHILD | WS_VISIBLE,
        kPadding, y + 4, 50, kLabelHeight,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessage(folder_label_, WM_SETFONT, (WPARAM)font_label_, TRUE);

    // Folder Combo
    folder_combo_ = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        kPadding + 55, y, width - kPadding * 2 - 55, kFieldHeight + 200,
        hwnd_, (HMENU)ID_FOLDER_COMBO, hInstance, nullptr
    );
    SendMessage(folder_combo_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
    SetWindowLongPtr(folder_combo_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    original_combo_proc_ = (WNDPROC)SetWindowLongPtr(folder_combo_, GWLP_WNDPROC,
                                                       reinterpret_cast<LONG_PTR>(ComboProc));
    y += kFieldHeight + kSpacing + 12;

    // Buttons at bottom
    int buttonY = clientRect.bottom - kPadding - kButtonHeight;

    // Remove button (left side, only in edit mode)
    remove_button_ = CreateWindowExW(
        0, L"BUTTON", L"Remove",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        kPadding, buttonY, kButtonWidth, kButtonHeight,
        hwnd_, (HMENU)ID_REMOVE_BUTTON, hInstance, nullptr
    );
    SendMessage(remove_button_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    // Cancel button
    cancel_button_ = CreateWindowExW(
        0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        width - kPadding - kButtonWidth * 2 - kSpacing, buttonY, kButtonWidth, kButtonHeight,
        hwnd_, (HMENU)ID_CANCEL_BUTTON, hInstance, nullptr
    );
    SendMessage(cancel_button_, WM_SETFONT, (WPARAM)font_normal_, TRUE);

    // Save button (default)
    save_button_ = CreateWindowExW(
        0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        width - kPadding - kButtonWidth, buttonY, kButtonWidth, kButtonHeight,
        hwnd_, (HMENU)ID_SAVE_BUTTON, hInstance, nullptr
    );
    SendMessage(save_button_, WM_SETFONT, (WPARAM)font_normal_, TRUE);
}

void BookmarkEditDialog::OnPaint() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);

    HDC hdc = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(DesignSystem::GetBackgroundColor());
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, hdc);
}

void BookmarkEditDialog::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    switch (id) {
        case ID_FOLDER_COMBO:
            if (code == CBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessage(folder_combo_, CB_GETCURSEL, 0, 0));
                if (sel == kNewFolderIndex) {
                    // "New Folder..." selected
                    ShowNewFolderDialog();
                }
            }
            break;
        case ID_SAVE_BUTTON:
            OnSave();
            break;
        case ID_CANCEL_BUTTON:
            dialog_result_ = Result::Cancel;
            DestroyWindow(hwnd_);
            break;
        case ID_REMOVE_BUTTON:
            OnRemove();
            break;
    }
}

void BookmarkEditDialog::OnClose() {
    dialog_result_ = Result::Cancel;
    DestroyWindow(hwnd_);
}

void BookmarkEditDialog::PopulateFolderPicker() {
    SendMessage(folder_combo_, CB_RESETCONTENT, 0, 0);

    // Add "No Folder" option
    SendMessageW(folder_combo_, CB_ADDSTRING, 0, (LPARAM)L"No Folder");

    // Add "New Folder..." option
    SendMessageW(folder_combo_, CB_ADDSTRING, 0, (LPARAM)L"New Folder...");

    // Get folders from storage
    folders_.clear();
    BookmarkStorage* storage = GetBookmarkStorage();
    if (storage) {
        folders_ = storage->GetFolders();
        if (!folders_.empty()) {
            // Add separator-like item (disabled)
            SendMessageW(folder_combo_, CB_ADDSTRING, 0, (LPARAM)L"───────────");

            for (const auto& folder : folders_) {
                SendMessageW(folder_combo_, CB_ADDSTRING, 0,
                            (LPARAM)DesignSystem::Utf8ToWide(folder).c_str());
            }
        }
    }
}

void BookmarkEditDialog::OnSave() {
    // Get values from fields
    int urlLen = GetWindowTextLengthW(url_edit_);
    int titleLen = GetWindowTextLengthW(title_edit_);
    int descLen = GetWindowTextLengthW(desc_edit_);

    std::wstring urlW(urlLen + 1, L'\0');
    std::wstring titleW(titleLen + 1, L'\0');
    std::wstring descW(descLen + 1, L'\0');

    GetWindowTextW(url_edit_, &urlW[0], urlLen + 1);
    GetWindowTextW(title_edit_, &titleW[0], titleLen + 1);
    GetWindowTextW(desc_edit_, &descW[0], descLen + 1);

    urlW.resize(urlLen);
    titleW.resize(titleLen);
    descW.resize(descLen);

    std::string url = DesignSystem::WideToUtf8(urlW);
    std::string title = DesignSystem::WideToUtf8(titleW);
    std::string description = DesignSystem::WideToUtf8(descW);

    // Validate URL
    if (url.empty()) {
        MessageBoxW(hwnd_, L"Please enter a URL for the bookmark.",
                   L"URL Required", MB_OK | MB_ICONWARNING);
        SetFocus(url_edit_);
        return;
    }

    // Add https:// if no scheme
    if (url.find("://") == std::string::npos) {
        url = "https://" + url;
    }

    // Use URL as title if title is empty
    if (title.empty()) {
        title = url;
    }

    // Get folder
    std::string folder = "";
    int sel = static_cast<int>(SendMessage(folder_combo_, CB_GETCURSEL, 0, 0));
    if (sel > 2 && sel - 3 < static_cast<int>(folders_.size())) {
        // Skip "No Folder" (0), "New Folder..." (1), separator (2)
        folder = folders_[sel - 3];
    }

    // Save bookmark
    BookmarkStorage* storage = GetBookmarkStorage();
    if (storage) {
        if (mode_ == BookmarkDialogMode::Add) {
            storage->AddBookmark(url, title, folder);
        } else {
            storage->UpdateBookmark(edit_bookmark_id_, title, folder);
        }

        if (on_save_) {
            on_save_();
        }
    }

    dialog_result_ = Result::Save;
    DestroyWindow(hwnd_);
}

void BookmarkEditDialog::OnRemove() {
    // Confirm deletion
    int result = MessageBoxW(hwnd_,
        L"Are you sure you want to remove this bookmark?",
        L"Remove Bookmark",
        MB_YESNO | MB_ICONQUESTION);

    if (result == IDYES) {
        BookmarkStorage* storage = GetBookmarkStorage();
        if (storage) {
            storage->DeleteBookmark(edit_bookmark_id_);

            if (on_save_) {
                on_save_();
            }
        }

        dialog_result_ = Result::Delete;
        DestroyWindow(hwnd_);
    }
}

void BookmarkEditDialog::ShowNewFolderDialog() {
    // Reset selection first (so we don't stay on "New Folder...")
    SendMessage(folder_combo_, CB_SETCURSEL, kNoFolderIndex, 0);

    // Get folder number suggestion
    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) return;

    int nextNum = storage->GetNextFolderNumber();
    std::wstring suggestedName = L"Collection " + std::to_wstring(nextNum);

    // Create a modal popup for folder name input
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Register a simple dialog class if needed
    static bool dialogClassRegistered = false;
    static const wchar_t* kFolderDialogClass = L"OrbFoxNewFolderDialog";

    if (!dialogClassRegistered) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = DefWindowProc;
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = kFolderDialogClass;
        RegisterClassExW(&wcex);
        dialogClassRegistered = true;
    }

    // Create a simple input dialog as a popup window
    const int dlgWidth = 300;
    const int dlgHeight = 130;

    RECT parentRect;
    GetWindowRect(hwnd_, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - dlgWidth) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - dlgHeight) / 2;

    HWND dlgHwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        kFolderDialogClass,
        L"New Folder",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dlgWidth, dlgHeight,
        hwnd_, nullptr, hInstance, nullptr
    );

    if (!dlgHwnd) return;

    // Create controls
    HFONT font = DesignSystem::CreateFont(DesignSystem::GetFontSizeBody());

    HWND labelHwnd = CreateWindowExW(
        0, L"STATIC", L"Enter folder name:",
        WS_CHILD | WS_VISIBLE,
        16, 16, 260, 20,
        dlgHwnd, nullptr, hInstance, nullptr
    );
    SendMessage(labelHwnd, WM_SETFONT, (WPARAM)font, TRUE);

    HWND editHwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", suggestedName.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        16, 40, 260, 24,
        dlgHwnd, (HMENU)1001, hInstance, nullptr
    );
    SendMessage(editHwnd, WM_SETFONT, (WPARAM)font, TRUE);

    HWND okBtn = CreateWindowExW(
        0, L"BUTTON", L"Create",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        dlgWidth - 180, dlgHeight - 60, 75, 28,
        dlgHwnd, (HMENU)IDOK, hInstance, nullptr
    );
    SendMessage(okBtn, WM_SETFONT, (WPARAM)font, TRUE);

    HWND cancelBtn = CreateWindowExW(
        0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        dlgWidth - 95, dlgHeight - 60, 75, 28,
        dlgHwnd, (HMENU)IDCANCEL, hInstance, nullptr
    );
    SendMessage(cancelBtn, WM_SETFONT, (WPARAM)font, TRUE);

    ShowWindow(dlgHwnd, SW_SHOW);
    SetFocus(editHwnd);
    SendMessage(editHwnd, EM_SETSEL, 0, -1);

    // Disable parent windows
    EnableWindow(hwnd_, FALSE);

    // Run modal loop for the folder dialog
    bool dialogDone = false;
    bool dialogOk = false;
    std::wstring folderNameResult;

    MSG msg;
    while (!dialogDone && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(dlgHwnd)) {
            dialogDone = true;
            break;
        }

        // Handle dialog-level messages
        if (msg.hwnd == dlgHwnd || IsChild(dlgHwnd, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_ESCAPE) {
                    dialogDone = true;
                    break;
                } else if (msg.wParam == VK_RETURN) {
                    dialogOk = true;
                    int len = GetWindowTextLengthW(editHwnd);
                    folderNameResult.resize(len + 1);
                    GetWindowTextW(editHwnd, &folderNameResult[0], len + 1);
                    folderNameResult.resize(len);
                    dialogDone = true;
                    break;
                }
            }

            if (msg.message == WM_COMMAND) {
                int id = LOWORD(msg.wParam);
                if (id == IDOK) {
                    dialogOk = true;
                    int len = GetWindowTextLengthW(editHwnd);
                    folderNameResult.resize(len + 1);
                    GetWindowTextW(editHwnd, &folderNameResult[0], len + 1);
                    folderNameResult.resize(len);
                    dialogDone = true;
                } else if (id == IDCANCEL) {
                    dialogDone = true;
                }
            }
        }

        if (!IsDialogMessage(dlgHwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Clean up
    DestroyWindow(dlgHwnd);
    DeleteObject(font);
    EnableWindow(hwnd_, TRUE);
    SetForegroundWindow(hwnd_);

    // Create the folder if OK was pressed and name is not empty
    if (dialogOk && !folderNameResult.empty()) {
        std::string newFolderName = DesignSystem::WideToUtf8(folderNameResult);

        if (storage->CreateFolder(newFolderName)) {
            // Repopulate the dropdown and select the new folder
            PopulateFolderPicker();

            // Find and select the new folder
            for (size_t i = 0; i < folders_.size(); i++) {
                if (folders_[i] == newFolderName) {
                    SendMessage(folder_combo_, CB_SETCURSEL, static_cast<int>(i) + 3, 0);
                    break;
                }
            }
        }
    }
}

#endif  // PLATFORM_WIN
