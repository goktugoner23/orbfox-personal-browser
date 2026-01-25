# Personal Browser

A custom web browser built with C++ and Chromium Embedded Framework (CEF), featuring a sidebar-based UI inspired by Vivaldi and Arc.

## Features

- Full Chromium rendering engine (CEF 144)
- Multi-process architecture for stability and security
- Native macOS application
- Keyboard shortcuts (Cmd+R reload, Cmd+[ back, Cmd+] forward)
- Context menu navigation
- Error page display

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
│   │   └── browser_app.cpp     # CefApp implementation
│   ├── client/
│   │   └── browser_client.cpp  # CefClient and handlers
│   └── platform/
│       └── mac/
│           ├── main_mac.mm     # macOS entry point
│           └── process_helper_mac.cc  # Helper process entry
├── include/
│   ├── browser_app.h
│   └── browser_client.h
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

Helper apps in `Contents/Frameworks/` handle subprocess execution:
- Personal Browser Helper.app
- Personal Browser Helper (GPU).app
- Personal Browser Helper (Renderer).app
- Personal Browser Helper (Plugin).app
- Personal Browser Helper (Alerts).app

## Roadmap

See [documentation/tasks.md](documentation/tasks.md) for the full implementation plan.

**Current Phase**: Foundation (Phase 1)
**Next Milestone**: Tab Management (Phase 2)

## License

MIT
