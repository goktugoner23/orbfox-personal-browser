# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- **History Panel Search** - Filter history entries with search field
  - Search field at top of history panel
  - Real-time filtering as you type
  - Searches both URLs and page titles
- **URL Bar Autocomplete** - Smart suggestions while typing in address bar
  - Dropdown appears below URL bar with matching suggestions
  - Shows bookmarks first, then history entries
  - Domain-based filtering (typing "youtube" only shows youtube.com pages)
  - Displays favicons for each suggestion
  - Keyboard navigation with up/down arrows
  - Click or Enter to navigate to selected URL
- **URL Bar Horizontal Scrolling** - Two-finger trackpad swipe to scroll long URLs
  - Works when URL field is focused
  - Smooth cursor-based scrolling through text
- **Tab Mute/Unmute** - Mute audio on individual tabs
  - Right-click tab → "Mute Tab" / "Unmute Tab"
  - Muted speaker icon displays on muted tabs
  - Uses CEF's native audio muting API
- **Persistent Favicon Cache** - Favicons saved to disk and restored on app launch
  - Stored in `~/Library/Application Support/OrbFox/cache/{domain}.png`
  - Automatically cached when visiting pages in tabs
  - Shared across bookmarks and history with same domain
- **Persistent Title Cache** - Page titles saved for bookmark display
  - Stored in `~/Library/Application Support/OrbFox/cache/titles.plist`
  - Bookmarks show page title (e.g., "Google") instead of URL
- **Add Bookmark Popover** - Full bookmark creation dialog
  - URL field (Address)
  - Nickname field (custom display name)
  - Description field (notes)
  - Folder picker dropdown
  - Accessible via "+" button → "Add Bookmark..."
- **Plus Button Context Menu** in bookmarks panel
  - "Add Bookmark..." - opens bookmark creation popover
  - "New Folder..." - creates new bookmark folder
- **Bookmark Double-Click** - Opens bookmark URL in new tab
- **Bookmark Right-Click Menu** - Context menu with Open, Edit, Move, Delete options
- **New Folder Button** in bookmarks panel
  - Creates empty bookmark folders for organization
  - Modal dialog with folder name input
  - Default name: "Collection 1", "Collection 2", etc. (auto-incrementing)
  - Validates for duplicate folder names
- **Bookmarks Menu** in menu bar
  - "Bookmark This Page" (Cmd+D) - toggle bookmark for current page
  - "New Folder..." (Cmd+Shift+N) - create new bookmark folder
  - "Show Bookmarks" (Cmd+Shift+B) - open bookmarks panel
  - Dynamic bookmark list - shows all bookmarks and folders
  - Click to open bookmark in new tab
  - Folders shown as submenus with their contents
- **History Menu** in menu bar
  - "Show History" (Cmd+Y) - open history panel
  - Back/Forward navigation
  - Dynamic history list - shows 15 most recent entries
  - Click to open history entry in new tab
- **Folder Management Methods** - BookmarkStorage API
  - CreateFolder(), FolderExists(), DeleteFolder(), RenameFolder()
  - GetNextFolderNumber() for "Collection N" naming

### Changed
- **Folder Header Design** - Improved collection folder appearance
  - Gray folder icons (was blue)
  - Chevron button for collapse/expand (clickable)
  - "Open All" button on right side (opens all bookmarks in new tabs)
  - Proper vertical alignment of icons and text
- **Bookmark Row Display** - Shows domain name when no nickname set
  - Falls back to cached page title, then domain name
  - Globe icon for bookmarks without cached favicon
- **Real-time Bookmark Updates** - Favicons and titles update live
  - When visiting a page, matching bookmarks update immediately
  - No need to reload bookmarks panel
- Total tests: 189 (was 171) - added 18 folder management tests

### Fixed
- **Folder Deletion** - Now properly deletes folders and their bookmarks
- **Empty Folder Deletion** - Deletes without confirmation dialog
- **Bookmark List Refresh** - List no longer disappears after folder deletion

## [0.7.0] - 2026-01-26

### Added
- **Enhanced Tab Context Menu**
  - "Rename Tab" - edit tab title via dialog
  - "Open in Space →" - submenu showing all workspaces + "New Space"
  - Duplicates tab to selected workspace
- **Workspace Bookmarking** - right-click workspace → "Bookmark Space"
  - Saves all tabs as bookmarks in a folder named after workspace
  - Dialog for duplicate folder name: Merge / New Folder / Cancel
  - Filters invalid URLs (empty, about:blank, chrome://)
- **Bookmark Folder UI** - grouped bookmark display
  - Collapsible folders with chevron icons
  - Folder headers with folder icon and "Open All" button
  - Indented bookmark rows under folders
  - Right-click folder menu: Open All, Rename Folder, Delete Folder
- **Integration Tests** - 25 new tests for tab manager
  - Multi-workspace tab operations
  - Workspace lifecycle tests
  - Callback sequence verification
  - Edge case coverage

### Changed
- Bookmarks panel now groups by folder with visual hierarchy

### Technical
- New `tab_manager_integration_test.cpp` with 25 tests
- Added `reloadWorkspaceTabs` to SidebarView header
- Bookmark folder state tracked via `_collapsedFolders` set

## [0.6.0] - 2026-01-26

### Added
- **Unit Testing Infrastructure** - GoogleTest-based test suite
  - 146 unit tests covering core components
  - Tests run without CEF runtime dependencies
  - Download manager tests (36 tests) - progress calculation, state transitions
  - Tab manager tests (48 tests) - workspace/tab CRUD, callbacks
  - Persistence tests (17 tests) - history, window settings, session storage
  - Bookmark storage tests (45 tests) - CRUD, folders, positions, edge cases
- **Test Build Target** - `OrbFoxTests` executable
  - GoogleTest fetched via CMake FetchContent
  - Isolated from main app with `UNIT_TEST` preprocessor guards

### Technical
- New `tests/` directory with test files
- `UNIT_TEST` guards in `tab.h` and `tab_manager.h` to exclude CEF types
- CMakeLists.txt updated with test configuration
- Added `ClearAllBookmarks()` method to BookmarkStorage for test isolation

## [0.5.0] - 2026-01-26

### Added
- **Download Progress Ring** - Rounded rectangle progress indicator around downloads icon
  - Matches icon button shape (not circular)
  - Only shows when file size is known
  - Fills from top-center going clockwise
- **Download Persistence** - Download history saved to disk
  - Stored in `~/Library/Application Support/OrbFox/downloads.json`
  - Completed/stopped downloads restored on app launch
- **File Existence Checking** - Detects when downloaded files are deleted
  - Shows "File deleted - double-click to re-download" status
  - Grayed out filename and remove button (no folder button)
  - Double-click to restart download
- **Download Restart Improvements**
  - Stopped downloads can be restarted with double-click
  - Remembers Save/Save As preference for restarts
  - New downloads always show confirmation dialog
  - Original URL preserved for display and restart
- **Downloads in History** - Download URLs added to browsing history
- **Responsive Sidebar Panels** - Download and history rows resize with sidebar
  - All elements properly anchored for resize

### Changed
- Progress bar only shown when download size is known
- Progress ring hidden when size is unknown (no fake progress)
- Stop button keeps download in list as "Stopped" (not removed)
- "Remove from List" only removes entry, doesn't cancel active download
- Status text shows only received bytes and speed when size unknown

### Fixed
- Fixed display of "-1%" when download percentage unknown
- Fixed deprecated `openFile:` warning (now uses `openURL:`)
- Fixed unused variable warnings in SidebarView

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
- **Bookmarks Panel** - Full bookmark management
  - SQLite-based bookmark storage (`~/Library/Application Support/OrbFox/bookmarks.db`)
  - Add current page with + button in panel
  - Click bookmark to open, close button to delete
  - Empty state message when no bookmarks
- **Toolbar Bookmark Button** - Quick bookmark toggle in address bar
  - Bookmark ribbon icon in actions area (right of URL bar)
  - Filled icon when page is bookmarked
  - Click to toggle bookmark state
- **Real-time History** - History panel updates live as you browse
  - New entries appear immediately when navigating to pages
  - No need to reopen panel to see recent history
- **Downloads Panel** - Full download management (Vivaldi-style)
  - Download confirmation dialog before download starts
    - Shows filename and file size
    - Save button (saves to ~/Downloads)
    - Save As button (opens file dialog)
    - Cancel button
    - Auto-renames file if exists (adds number suffix)
  - Real-time progress bar with percentage
  - Download speed display (KB/s, MB/s)
  - File size info (received/total bytes)
  - Show in Finder button for completed downloads
  - Clear completed button to clean up list
  - Status indicators (in progress, complete, canceled, failed)

### Changed
- Sidebar bookmark icon changed from star to ribbon (matches toolbar)
- Sidebar icons use `selected` state for active panel (persistent background)
- Sidebar icons don't show hover background (only tint change on hover)
- Workspace selector changed from dropdown to horizontal tab bar
- Default workspace renamed from "Personal" to "WS 1"
- Pin icon and close button share same position (swap on hover)

### Technical
- New `session_storage.h/cpp` for JSON-based session persistence
- New `bookmark_storage.h/cpp` for SQLite-based bookmark storage
- New `download_manager.h/cpp` for tracking downloads across browsers
- Added resize handle view for sidebar resizing
- Workspace tabs use horizontal NSScrollView
- `DSIconButton` gains `selected` and `showsHoverBackground` properties
- `DSButton` gains `resetHoverState` method
- Toolbar has actions area for bookmark button and future extensions
- `BrowserClient` implements `CefDownloadHandler` for download support
- Download callbacks stored for cancel/pause/resume functionality

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
