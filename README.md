# Personal Browser

A custom web browser built with C++ and Chromium Embedded Framework (CEF), featuring a sidebar-based UI inspired by Vivaldi and Arc.

## UI Design

```
┌─────────────────────────────────────────────────────────┐
│ ● ● ●  Page Title                                       │
├────┬────────────────┬───────────────────────────────────┤
│ 📑 │ ● Personal  +  │                                   │
│ ⭐ │────────────────│                                   │
│ 🕐 │ Tab 1        × │         Web Content               │
│ ⬇️ │ Tab 2        × │                                   │
│    │                │                                   │
│    │                │                                   │
│    │                │                                   │
│    │ + New Tab      │                                   │
├────┴────────────────┼───────────────────────────────────┤
│                     │  < > ↻  https://example.com       │
└─────────────────────┴───────────────────────────────────┘
```

- **Left sidebar**: Panel icons, workspace selector, vertical tabs
- **Bottom toolbar**: Navigation buttons + URL bar
- **Dark theme** by default

## Features

- Full Chromium rendering engine (CEF 144)
- Multi-process architecture for stability and security
- Native macOS application with custom Cocoa UI
- **Vivaldi-style sidebar** with vertical tabs
- **Bottom toolbar** with navigation and URL bar
- **Workspace support** for organizing tabs
- Window position/size persistence
- Keyboard shortcuts (Cmd+R reload, Cmd+[ back, Cmd+] forward)
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
open build/PersonalBrowser.app
```

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
│   │   └── window_settings.cpp # Window persistence
│   ├── ui/
│   │   ├── MainWindowController.mm  # Main window logic
│   │   ├── SidebarView.mm      # Sidebar with tabs/workspaces
│   │   └── ToolbarView.mm      # Bottom navigation bar
│   └── platform/
│       └── mac/
│           ├── main_mac.mm     # macOS entry point
│           └── process_helper_mac.cc  # Helper process entry
├── include/
│   ├── browser_app.h
│   ├── browser_client.h
│   ├── tab.h                   # Tab data model
│   ├── tab_manager.h           # Tab management interface
│   ├── window_settings.h
│   └── workspace.h             # Workspace data model
├── resources/
│   ├── Info.plist              # App bundle info
│   └── helper-Info.plist.in    # Helper app template
└── documentation/
    └── tasks.md                # Implementation roadmap
```

## Architecture

The browser uses CEF's multi-process architecture:

- **Main Process**: UI, browser management, application logic
- **GPU Process**: Hardware-accelerated rendering
- **Renderer Process**: Web content rendering (one per tab)
- **Network Process**: Network requests
- **Utility Processes**: Audio, storage, etc.

Helper apps in `Contents/Frameworks/` handle subprocess execution.

## Roadmap

See [documentation/tasks.md](documentation/tasks.md) for the full implementation plan.

**Current Phase**: Tab Management (Phase 2)
**Completed**: Foundation (Phase 1)

## License

MIT
