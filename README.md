# OrbFox Browser

A custom web browser built with C++ and Chromium Embedded Framework (CEF), featuring a sidebar-based UI inspired by Vivaldi and Arc.

## UI Design

```
┌─────────────────────────────────────────────────────────┐
│ ● ● ●  Page Title                                       │
├────┬────────────────┬───────────────────────────────────┤
│ 📑 │ WS 1  WS 2  +  │                                   │
│ ⭐ │────────────────│                                   │
│ 🕐 │ 📌 Tab 1     × │         Web Content               │
│ ⬇️ │ Tab 2        × │                                   │
│    │                │                                   │
│    │                │                                   │
│    │                │                                   │
│    │ + New Tab      │                                   │
├────┴────────────────┼───────────────────────────────────┤
│                     │  < > ↻  🔒 https://example.com    │
└─────────────────────┴───────────────────────────────────┘
```

- **Left sidebar**: Panel icons, horizontal workspace tabs, vertical tabs
- **Resizable sidebar**: Drag to resize between 200-400px
- **Bottom toolbar**: Navigation buttons + URL bar with security indicator
- **Dark theme** by default with consistent design system

## Features

- Full Chromium rendering engine (CEF 144)
- Multi-process architecture for stability and security
- Native macOS application with custom Cocoa UI
- **Vivaldi-style sidebar** with vertical tabs
- **Resizable sidebar** - drag to resize (200-400px)
- **Bottom toolbar** with navigation and URL bar
- **Arc-style workspaces** - horizontal scrollable workspace tabs
  - Create, rename, duplicate, delete workspaces
  - Right-click context menu on workspace tabs
  - Close button on each workspace tab
- **Tab pinning** - pin tabs with visual indicator, warning on workspace close
- **Session persistence** - restore workspaces and tabs on restart (Vivaldi-style)
- **Bookmarks panel** - full bookmark management
  - Add bookmarks via popover with URL, nickname, description
  - Folder organization with collapsible collections
  - Persistent favicon and title cache
  - Double-click to open, right-click for context menu
- **Collapsible sidebar** - click active panel icon to toggle
- **History panel** with browsing history
  - Search/filter history entries in real-time
- **URL bar autocomplete** - suggestions from bookmarks and history
  - Domain-based filtering (shows only relevant domains)
  - Favicon display for each suggestion
  - Keyboard navigation support
- **URL bar horizontal scrolling** - two-finger swipe to scroll long URLs
- **Menu bar integration** - dynamic bookmark and history menus
  - Bookmarks menu shows all bookmarks with folders as submenus
  - History menu shows 15 most recent entries
  - Click any entry to open in new tab
- **Downloads panel** - full download management
  - Progress bar and icon ring (only when size is known)
  - Stop/restart downloads, remembered save preferences
  - Detects deleted files, allows re-download
  - Persistent download history
- **Favicons** displayed in tab list
- **Loading indicators** with animated progress bar
- **Responsive sidebar** - panels resize with sidebar width
- Window position/size persistence
- Keyboard shortcuts (Cmd+T, Cmd+W, Cmd+L, Cmd+1-9, etc.)
- Context menu navigation
- Dark theme throughout

## Requirements

- macOS 12.0 or later (ARM64)
- CMake 3.15+
- Xcode Command Line Tools

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j8

# Run
open build/OrbFox.app
```

## Testing

Unit tests cover core components without requiring CEF runtime:

```bash
# Configure with tests
cmake -B build-test -DCMAKE_BUILD_TYPE=Debug

# Build tests
cmake --build build-test --target OrbFoxTests

# Run all tests
./build-test/OrbFoxTests

# Run specific test suite
./build-test/OrbFoxTests --gtest_filter="DownloadManagerTest.*"
```

**Test coverage:**
- Download manager (36 tests) - progress calculation, state transitions, cancel/pause
- Tab manager (48 tests) - workspace/tab CRUD, active tracking, callbacks
- Tab manager integration (25 tests) - multi-workspace operations, callback sequences
- Persistence (17 tests) - history storage, window settings, session storage
- Bookmark storage (63 tests) - CRUD, folders, positions, edge cases

## Project Structure

```
personal-browser/
├── CMakeLists.txt              # Build configuration
├── cef/                        # CEF binary distribution
├── src/
│   ├── main.cpp                # Entry point stub
│   ├── app/
│   │   └── browser_app.mm      # CefApp + window creation
│   ├── browser/
│   │   └── tab_manager.cpp     # Tab and workspace management
│   ├── client/
│   │   └── browser_client.cpp  # CefClient and handlers
│   ├── data/
│   │   ├── window_settings.cpp # Window persistence
│   │   ├── history_storage.cpp # Browsing history (SQLite)
│   │   └── session_storage.cpp # Session persistence (JSON)
│   ├── ui/
│   │   ├── components/         # Reusable UI components
│   │   │   ├── DesignSystem.h/mm   # Colors, typography, spacing
│   │   │   ├── DSButton.h/mm       # Button component
│   │   │   ├── DSTextField.h/mm    # Text field component
│   │   │   ├── DSRow.h/mm          # List row component
│   │   │   └── Components.h        # Component index
│   │   ├── MainWindowController.mm # Main window logic
│   │   ├── SidebarView.mm      # Sidebar with tabs/workspaces
│   │   └── ToolbarView.mm      # Bottom navigation bar
│   └── platform/
│       └── mac/
│           ├── main_mac.mm     # macOS entry point + menu bar
│           └── process_helper_mac.cc  # Helper process entry
├── include/
│   ├── browser_app.h
│   ├── browser_client.h
│   ├── tab.h                   # Tab data model
│   ├── tab_manager.h           # Tab management interface
│   ├── history_storage.h       # History storage interface
│   ├── session_storage.h       # Session persistence interface
│   ├── window_settings.h
│   └── workspace.h             # Workspace data model
├── resources/
│   ├── Info.plist              # App bundle info
│   └── helper-Info.plist.in    # Helper app template
├── tests/
│   ├── download_manager_test.cpp  # Download state/progress tests
│   ├── tab_manager_test.cpp       # Tab/workspace management tests
│   └── persistence_test.cpp       # Storage layer tests
└── .claude/
    └── skills/                 # Claude Code skills
        ├── native-ui-design/   # UI design guidelines
        ├── cef-browser/        # CEF development guide
        └── cpp-development/    # C++ best practices
```

## Component Architecture

The UI is built with a React-inspired component system:

### Design System (`src/ui/components/DesignSystem.h`)

Centralized theme configuration:

```objc
// Colors
[DSColors background]      // Main background
[DSColors textPrimary]     // Primary text
[DSColors accent]          // Accent color

// Typography
[DSTypography fontWithStyle:DSFontStyleBody]

// Spacing (4pt grid)
[DSSpacing sm]  // 8pt
[DSSpacing md]  // 12pt

// Layout
[DSLayout cornerRadiusMedium]  // 6pt
[DSLayout iconSizeLarge]       // 28pt
```

### Reusable Components

- **DSButton** - Button with variants (Ghost, Subtle, Filled) and hover effects
- **DSIconButton** - Icon-only button with SF Symbols
- **DSTextField** - Text field with focus ring
- **DSRow** - List row with hover effect (base for tabs, history items)

## Architecture

The browser uses CEF's multi-process architecture:

- **Main Process**: UI, browser management, application logic
- **GPU Process**: Hardware-accelerated rendering
- **Renderer Process**: Web content rendering (one per tab)
- **Network Process**: Network requests
- **Utility Processes**: Audio, storage, etc.

Helper apps in `Contents/Frameworks/` handle subprocess execution.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Cmd+T | New tab |
| Cmd+W | Close tab |
| Cmd+L | Focus URL bar |
| Cmd+R | Reload |
| Cmd+[ | Back |
| Cmd+] | Forward |
| Cmd+1-9 | Switch to tab |
| Cmd+D | Bookmark this page |
| Cmd+Shift+N | New bookmark folder |
| Cmd+Shift+Y | Show History |
| Cmd+Shift+B | Show Bookmarks |

## License

MIT
