# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.1.0] - 2025-01-26

### Added
- Initial project setup with CMake build system
- CEF 144.0.11 integration for macOS ARM64
- BrowserApp implementation (CefApp + CefBrowserProcessHandler)
- BrowserClient with full handler support:
  - CefLifeSpanHandler - browser lifecycle management
  - CefLoadHandler - page load events and error handling
  - CefDisplayHandler - title and URL change notifications
  - CefRequestHandler - navigation control
  - CefContextMenuHandler - right-click context menu
  - CefKeyboardHandler - keyboard shortcut handling
- macOS helper apps for multi-process architecture:
  - Personal Browser Helper.app (main)
  - Personal Browser Helper (GPU).app
  - Personal Browser Helper (Renderer).app
  - Personal Browser Helper (Plugin).app
  - Personal Browser Helper (Alerts).app
- CEF Views framework integration for native window
- Basic navigation support (back, forward, reload, URL loading)
- Keyboard shortcuts:
  - Cmd+R - Reload page
  - Cmd+[ - Navigate back
  - Cmd+] - Navigate forward
- Context menu with navigation options
- Custom error page for failed loads
- Project documentation and task roadmap

### Technical
- C++17 standard
- Multi-process architecture (not single-process mode)
- Proper RAII and CEF reference counting patterns
- macOS 12.0+ deployment target
