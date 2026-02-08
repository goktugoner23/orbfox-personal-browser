// Resource IDs for OrbFox on Windows

#pragma once

// Icons
#define IDI_ORBFOX 100

// Menu IDs
#define IDM_FILE            10000
#define IDM_FILE_NEWTAB     10001
#define IDM_FILE_CLOSETAB   10002
#define IDM_FILE_EXIT       10003

#define IDM_EDIT            10100
#define IDM_EDIT_UNDO       10101
#define IDM_EDIT_REDO       10102
#define IDM_EDIT_CUT        10103
#define IDM_EDIT_COPY       10104
#define IDM_EDIT_PASTE      10105
#define IDM_EDIT_SELECTALL  10106

#define IDM_VIEW            10200
#define IDM_VIEW_TABS       10201
#define IDM_VIEW_BOOKMARKS  10202
#define IDM_VIEW_HISTORY    10203
#define IDM_VIEW_DOWNLOADS  10204
#define IDM_VIEW_RELOAD     10205

#define IDM_HELP            10300
#define IDM_HELP_ABOUT      10301

// Accelerator IDs
#define IDA_NEWTAB          200
#define IDA_CLOSETAB        201
#define IDA_RELOAD          202
#define IDA_FOCUSURL        203

// Custom window message for forwarding keyboard shortcuts from CEF to MainWindow
// Used by BrowserClient::OnPreKeyEvent to forward shortcuts when browser has focus
#define WM_ORBFOX_SHORTCUT  (WM_APP + 1)

// Shortcut action IDs (sent as WPARAM of WM_ORBFOX_SHORTCUT)
#define SHORTCUT_NEW_TAB          1
#define SHORTCUT_CLOSE_TAB        2
#define SHORTCUT_FIND             3
#define SHORTCUT_REOPEN_TAB       4
#define SHORTCUT_TOGGLE_DEVTOOLS  5
#define SHORTCUT_PREV_WORKSPACE   6
#define SHORTCUT_NEXT_WORKSPACE   7
#define SHORTCUT_SWITCH_TAB       8   // LPARAM = tab index (0-8)
