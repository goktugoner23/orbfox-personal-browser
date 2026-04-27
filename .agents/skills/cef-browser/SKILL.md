---
name: cef-browser
description: Guide for building C++ browser applications using Chromium Embedded Framework (CEF). Use when working with CEF initialization, CefApp, CefClient, browser handlers (CefLifeSpanHandler, CefLoadHandler, CefDisplayHandler), CefBrowser lifecycle, CMake configuration for CEF projects, or cross-platform (macOS/Windows) browser development. Covers modern C++17 patterns, memory management with CefRefPtr, and RAII practices.
---

# CEF Browser Development

## Overview

Build native browser applications using CEF (Chromium Embedded Framework) with full Chrome compatibility. CEF uses a multi-process architecture: Browser (main), Renderer (sandboxed), GPU, and Utility processes.

## Core Classes

### CefApp - Application-level (one per process)

```cpp
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnContextInitialized() override {
        // CEF ready - create browser windows here
    }

    IMPLEMENT_REFCOUNTING(BrowserApp);
};
```

### CefClient - Per-browser callbacks

```cpp
class BrowserClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler {
public:
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    IMPLEMENT_REFCOUNTING(BrowserClient);
};
```

## Handler Implementations

### CefLifeSpanHandler - Browser lifecycle

```cpp
void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    // Browser created - store reference if needed
}

bool DoClose(CefRefPtr<CefBrowser> browser) override {
    return false;  // false = allow close
}

void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    // Cleanup - call CefQuitMessageLoop() when last browser closes
}
```

### CefLoadHandler - Page loading

```cpp
void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                          bool isLoading, bool canGoBack, bool canGoForward) override;

void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                 ErrorCode errorCode, const CefString& errorText,
                 const CefString& failedUrl) override;
```

### CefDisplayHandler - Display events

```cpp
void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                     const CefString& url) override;
```

## Initialization

```cpp
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    // Handle subprocess
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) return exit_code;

    CefSettings settings;
    settings.no_sandbox = true;  // Development only

    CefRefPtr<BrowserApp> app(new BrowserApp);
    CefInitialize(main_args, settings, app, nullptr);
    CefRunMessageLoop();
    CefShutdown();
    return 0;
}
```

## Creating Browsers

```cpp
void CreateBrowser(const std::string& url, void* native_parent, const CefRect& bounds) {
    CefWindowInfo window_info;
#ifdef __APPLE__
    // native_parent is an NSView* bridged to void*.
    window_info.SetAsChild(native_parent, bounds);
#elif defined(_WIN32)
    // native_parent is an HWND.
    window_info.SetAsChild(native_parent, bounds);
#endif

    CefBrowserSettings settings;
    CefRefPtr<BrowserClient> client(new BrowserClient);
    CefBrowserHost::CreateBrowser(window_info, client, url, settings, nullptr, nullptr);
}
```

## Navigation

```cpp
browser->GetMainFrame()->LoadURL("https://example.com");
browser->GoBack();
browser->GoForward();
browser->Reload();
browser->StopLoad();
browser->GetHost()->CloseBrowser(false);
```

## C++ Best Practices

### Memory Management

```cpp
// CEF objects: use CefRefPtr
CefRefPtr<CefBrowser> browser;

// Application objects: use smart pointers
auto window = std::make_unique<BrowserWindow>(config);
auto manager = std::make_shared<TabManager>();

// NEVER use raw new/delete
```

### RAII Pattern

```cpp
class BrowserResource {
    CefRefPtr<CefBrowser> browser_;
public:
    explicit BrowserResource(CefRefPtr<CefBrowser> b) : browser_(b) {}
    ~BrowserResource() {
        if (browser_) browser_->GetHost()->CloseBrowser(true);
    }
    BrowserResource(const BrowserResource&) = delete;
    BrowserResource& operator=(const BrowserResource&) = delete;
};
```

## CMake Configuration

For detailed CMake setup, see [references/cmake-setup.md](references/cmake-setup.md).

Basic structure:

```cmake
cmake_minimum_required(VERSION 3.15...4.0)
project(MyBrowser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CEF_ROOT "${CMAKE_SOURCE_DIR}/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_executable(MyBrowser src/main.cpp)
target_link_libraries(MyBrowser PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
```

## Platform Specifics

### macOS
- Use NSApplication for event loop or `CefRunMessageLoop()`
- Browser views are NSViews in NSWindows
- Use `.mm` extension for Objective-C++ files
- Code signing required for distribution

### Windows
- Win32 message loop integration
- Browser windows are HWNDs
- Handle high-DPI support

## Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MyBrowser
```

## Common Issues

**App quits unexpectedly on browser close:**
- Only call `CefQuitMessageLoop()` when main window closes
- Use explicit shutdown flags, not browser counters
- Track window close vs tab close separately

**Build errors:**
- Ensure CEF_ROOT points to extracted CEF distribution
- Include CEF cmake modules in CMAKE_MODULE_PATH
- Link against libcef_dll_wrapper

## References

- [references/cmake-setup.md](references/cmake-setup.md) - Detailed CMake configuration
- [references/handlers.md](references/handlers.md) - Complete handler reference
- CEF wiki: https://bitbucket.org/chromiumembedded/cef/wiki
