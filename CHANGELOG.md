# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.2.0] - 2026-01-26

### Added
- Native Cocoa UI replacing CEF Views framework
- Vivaldi-style sidebar with vertical tab list
- Bottom toolbar with navigation buttons and URL bar
- Workspace system for tab organization
- Tab management (create, close, switch tabs)
- Icon strip in sidebar (tabs, favorites, history, downloads)
- New Tab button in sidebar
- Add workspace button

### Changed
- Switched from CEF Views to native AppKit/Cocoa for UI
- URL field now uses container view for proper sizing
- Sidebar extends full height of window

### UI
- Dark theme with consistent colors
- Vertically centered URL text
- Properly sized New Tab button matching toolbar height

## [0.1.0] - 2026-01-26

### Added
- Initial project setup with CMake build system
- CEF 144.0.11 integration for macOS ARM64
- Multi-process architecture with helper apps
- CEF Views framework for native window
- Window position/size persistence (`~/Library/Application Support/PersonalBrowser/`)
- Basic navigation (back, forward, reload, URL loading)
- Keyboard shortcuts: Cmd+R (reload), Cmd+[ (back), Cmd+] (forward)
- Context menu with navigation options
- Custom error page for failed loads

### Technical
- C++17 standard
- macOS 12.0+ deployment target (ARM64)
- Proper RAII and CEF reference counting patterns
