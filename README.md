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
  - Right-click context menu on workspace tabs (rename, duplicate, delete, change color)
  - Color-coded workspace tabs (8-color palette with dot indicator)
  - Close button on each workspace tab
- **Tab pinning** - pin tabs with visual indicator, warning on workspace close
- **Tab muting** - mute/unmute tabs via right-click menu, speaker icon indicator
- **Reopen closed tabs** (Cmd+Shift+T) - restore recently closed tabs (up to 25)
- **Move tabs between workspaces** - right-click tab → Move to → workspace or "New Space"
- **Unique workspace naming** - auto-increment (WS 1, WS 2, WS 3...)
- **Link context menu** - right-click links to open in new tab, background, or copy address
- **Middle-click support** - mouse button 3 opens items in background tabs
  - Middle-click bookmarks, history, or web page links to open without switching
  - Middle-click tabs to duplicate in background
- **Mouse gestures** - Vivaldi-style right-click drag gestures (configurable in settings)
  - Drag left → Go back
  - Drag right → Go forward
  - L-shape (down then right) → Close current tab
  - Individual gesture toggles in orbfox://settings
- **Tab hibernation** - Automatic memory optimization
  - Tabs inactive for 5+ minutes are suspended
  - Browser closed, tab data preserved
  - Moon icon shows hibernated state
  - Pinned tabs never hibernate
- **Image context menu** - right-click images to save, copy address, or open in new tab
- **DevTools Panel** (Cmd+Opt+I) - Integrated Chrome DevTools
  - Slides in from right as embedded panel
  - Native header bar with close button
  - Resizable via drag divider
  - "Inspect Element" from context menu
- **Session persistence** - restore workspaces and tabs on restart (Vivaldi-style)
- **Crash recovery** - automatic session restore after unexpected shutdown
- **Crash reporting** - detailed crash reports saved to disk
- **Async browser lifecycle safety** - handles async CEF browser creation/destruction races gracefully
- **Bookmark import** - import bookmarks from Chrome, Safari, Firefox, Edge
- **Bookmarks panel** - full bookmark management
  - Add bookmarks via popover with URL, nickname, description
  - Folder organization with collapsible collections
  - Persistent favicon and title cache
  - Double-click to open, right-click for context menu
- **Settings page** (orbfox://settings) - browser preferences
  - General, Search, Privacy, Downloads, Gestures, About sections
  - Gear icon in sidebar for quick access
  - Gear favicon for internal pages
- **Collapsible sidebar** - click active panel icon to toggle
- **History panel** with browsing history
  - Search/filter history entries in real-time
  - Clear all history with confirmation
- **Find in Page** (Cmd+F) - search text within pages
  - Floating find bar with match count
  - Previous/Next navigation
- **Search shortcuts** - Keyword shortcuts in address bar
  - Type `y kitten vids` to search YouTube, `g query` for Google, `a item` for Amazon
  - Add/edit/delete custom shortcuts in orbfox://settings → Search
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
- **Loading indicators** with animated progress bar (debounced like Chrome/Safari)
- **Content fullscreen** - YouTube/HTML5 fullscreen hides browser UI for true fullscreen
- **Responsive sidebar** - panels resize with sidebar width
- Window position/size persistence
- Keyboard shortcuts (Cmd+T, Cmd+W, Cmd+L, Cmd+1-9, Cmd+Opt+Left/Right for workspace switching, etc.)
- Context menu navigation
- Dark theme throughout

## Requirements

### macOS
- macOS 12.0 or later (ARM64)
- CMake 3.15+
- Xcode Command Line Tools
- CEF Binary Distribution (see below)

### Windows
- Windows 10 or later (x64)
- CMake 3.15+
- Visual Studio 2019 or later (with C++ workload)
- CEF Binary Distribution (see below)

## CEF Setup

The Chromium Embedded Framework (CEF) binaries are **not included** in this repository due to their large size (~200 MB). You must download CEF separately before building.

### Version

| Component | Version |
|-----------|---------|
| CEF | `144.0.11+ge135be2+chromium-144.0.7559.97` |
| Chromium | `144.0.7559.97` |
| Branch | `144` |

Both macOS and Windows builds **must** use the same CEF version for consistency.

### macOS (ARM64)

1. Download from [CEF Builds](https://cef-builds.spotifycdn.com/index.html):
   - Platform: **macOS 64-bit ARM** (arm64)
   - Search for version `144.0.11` or branch `144`
   - Download the **Standard Distribution**

2. Extract and place in the project root:
   ```bash
   tar -xjf cef_binary_144.0.11*.tar.bz2
   mv cef_binary_144.0.11* cef
   ```

### Windows (x64)

1. Download from [CEF Builds](https://cef-builds.spotifycdn.com/index.html):
   - Platform: **Windows 64-bit** (x64)
   - Search for version `144.0.11` or branch `144`
   - Download the **Standard Distribution**

2. Extract and place in the project root:
   ```powershell
   # Extract the downloaded archive and rename to 'cef'
   mv cef_binary_144.0.11* cef
   ```

3. Install SQLite3 via vcpkg:
   ```powershell
   vcpkg install sqlite3:x64-windows
   ```

### Directory structure after setup:

```
personal-browser/
├── cef/
│   ├── cmake/
│   ├── include/
│   ├── libcef_dll/
│   ├── Release/
│   └── ...
├── src/
└── ...
```

## Building

### macOS

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j8

# Run
open build/OrbFox.app
```

### Windows

```powershell
# Configure (using Visual Studio generator)
# If using vcpkg for SQLite3, add: -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# Run
.\build\Release\OrbFox.exe
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

**Test coverage (284 tests):**
- Download manager (47 tests) - progress calculation, state transitions, cancel/pause, persistence
- Tab manager (73 tests) - workspace/tab CRUD, active tracking, callbacks, reopen closed, move tabs, unique naming, colors
- Tab manager integration (25 tests) - multi-workspace operations, callback sequences
- Persistence (22 tests) - history storage, window settings, session storage, tab properties
- Bookmark storage (63 tests) - CRUD, folders, positions, edge cases
- Settings storage (49 tests) - JSON serialization, path validation, security
- Filesystem utils (5 tests) - path validation, HTML escaping

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

| Action | macOS | Windows |
|--------|-------|---------|
| New tab | Cmd+T | Ctrl+T |
| Close tab | Cmd+W | Ctrl+W |
| Reopen closed tab | Cmd+Shift+T | Ctrl+Shift+T |
| Focus URL bar | Cmd+L | Ctrl+L |
| Find in Page | Cmd+F | Ctrl+F |
| Reload | Cmd+R | Ctrl+R |
| Back | Cmd+[ | Ctrl+[ |
| Forward | Cmd+] | Ctrl+] |
| Switch to tab 1-9 | Cmd+1-9 | Ctrl+1-9 |
| Toggle DevTools | Cmd+Opt+I | Ctrl+Alt+I / F12 |
| Previous workspace | Cmd+Opt+Left | Ctrl+Alt+Left |
| Next workspace | Cmd+Opt+Right | Ctrl+Alt+Right |

## License

MIT
