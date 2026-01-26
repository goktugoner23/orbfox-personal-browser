# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.4.0] - 2026-01-26

### Added
- **App Renamed** - Renamed from "Personal Browser" to "OrbFox"
- **Resizable Sidebar** - Drag handle to resize sidebar (200-400px range)
- **Arc-style Workspace Tabs** - Horizontal scrollable workspace tabs
  - Workspaces named "WS 1", "WS 2", etc. instead of "Personal"
  - Close button on each workspace tab
  - Right-click context menu (Rename, Duplicate, Delete)
  - Closing last workspace quits the browser
- **Session Persistence** - Vivaldi-style session restore
  - Saves workspaces and tabs on quit
  - Restores previous session on launch
  - Stored in `~/Library/Application Support/OrbFox/session.json`
- **Tab Pinning** - Pin/unpin tabs via right-click menu
  - Pin icon shown on pinned tabs (swaps with close button on hover)
  - Warning dialog when closing workspace with pinned tabs

### Changed
- Workspace selector changed from dropdown to horizontal tab bar
- Default workspace renamed from "Personal" to "WS 1"
- Pin icon and close button share same position (swap on hover)

### Technical
- New `session_storage.h/cpp` for JSON-based session persistence
- Added resize handle view for sidebar resizing
- Workspace tabs use horizontal NSScrollView

## [0.3.0] - 2026-01-26

### Added
- **Component Architecture** - React-inspired reusable UI components
  - `DSButton` with variants (Ghost, Subtle, Filled) and sizes
  - `DSIconButton` for icon-only buttons with SF Symbols
  - `DSTextField` with focus ring styling
  - `DSRow` base component for list items with hover effects
  - `DSHistoryRow` specialized for browsing history display
- **Design System** - Centralized theme configuration
  - `DSColors` - Semantic color palette (background, surface, text, accent, etc.)
  - `DSTypography` - Font styles (body, caption, title, etc.)
  - `DSSpacing` - 4pt grid spacing system (xs, sm, md, lg, xl)
  - `DSLayout` - Layout constants (corner radii, icon sizes, etc.)
  - `DSAnimation` - Animation helpers with standard durations
- Hover effects on all interactive elements (buttons, tabs, history rows)
- Collapsible sidebar - click active panel icon to toggle
- Menu bar integration for History, Bookmarks, Tabs, Downloads panels
- Keyboard shortcuts: Cmd+Shift+Y (History), Cmd+Shift+B (Bookmarks)
- Loading progress indicator in URL bar
- Security indicator icons (lock for HTTPS, warning for HTTP)

### Changed
- Refactored SidebarView to use component architecture
- Refactored ToolbarView to use component architecture
- All hardcoded colors replaced with Design System colors
- All hardcoded spacing replaced with Design System spacing
- Improved focus states with accent-colored borders

### Technical
- New `src/ui/components/` directory for reusable components
- `Components.h` index file for easy importing
- NSTrackingArea-based hover detection in all interactive components
- Block-based callbacks (onClick, onClose, onSubmit) for component events
- Fixed weak pointer handling in blocks to prevent crashes

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
