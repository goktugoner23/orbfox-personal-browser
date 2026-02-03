# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- **Image Context Menu** - Right-click on images for quick actions
  - "Save Image As..." - triggers download of the image
  - "Copy Image Address" - copies image URL to clipboard
  - "Open Image in New Tab" - opens image in new foreground tab
- **Mouse Gestures** - Vivaldi-style right-click drag gestures
  - Drag left → Go back
  - Drag right → Go forward
  - L-shape (down then right) → Close current tab
  - Minimum 50pt drag distance to trigger gesture
  - Falls back to context menu if no gesture detected
- **Context Menu Bookmark Options** - Right-click to add bookmarks
  - "Add Link to Bookmarks" appears when right-clicking links
  - "Bookmark This Page" available on all context menus
  - Opens bookmark popover for editing before saving
- **Bookmark Folder Drag & Drop** - Reorder folders by dragging
  - Drag folder headers to reorder within bookmark list
  - Folders and bookmarks share unified position system at root level
  - Visual drop indicator shows insertion point
- **Middle-Click Support** - Mouse button 3 opens items in background tabs
  - Middle-click on bookmarks in sidebar opens in background tab
  - Middle-click on history items opens in background tab
  - Middle-click on tabs duplicates tab to background
  - Middle-click on links in web pages opens in background tab
- **Reusable Browser Actions** - Centralized action methods on MainWindowController
  - `openUrlInCurrentTab:` - Navigate current tab to URL
  - `openUrlInNewTab:` - Open URL in new foreground tab
  - `openUrlInBackgroundTab:` - Open URL in new background tab (no switch)
  - `copyUrlToClipboard:` - Copy URL to system clipboard
  - All context menus and middle-click handlers use these centralized methods

### Changed
- **Edit Bookmark Dialog** - Now includes all bookmark fields
  - Address (URL), Nickname, Description, and Folder fields
  - Matches the Add Bookmark popover layout
  - Same polished UI with proper button positioning (Cancel left, Save right)
- **Internal Page Context Menu** - Reload/Stop hidden for orbfox:// pages
  - Context menu no longer shows Reload, Stop, or Bookmark options on internal pages
  - Matches toolbar behavior which already hides reload button for internal pages
- **Bookmark Folders Collapsed by Default** - Folders start collapsed on app launch
  - All folders are collapsed when first loading bookmarks panel
  - New folders also start collapsed (inverted tracking logic)
  - User can expand folders, state persists during session

### Fixed
- **Gesture Container Context Menu** - Fixed right-click showing no menu when not dragging
  - Mouse gestures intercepted right-click events but didn't forward to CEF when no gesture detected
  - Now properly forwards right-click to browser view to show context menu
- **Bookmark Drag & Drop Crash** - Fixed crash when dragging bookmarks out of folders
  - Null pointer crash in performDragOperation when moving to root level
- **Folder Positioning in Bookmark List** - Folders now correctly interleave with bookmarks
  - Folders were using loop index instead of database position for sorting
  - Now uses actual `GetFolderPosition()` for correct ordering
  - Folders can be moved to any position including bottom of list
- **Drop Indicator Position** - Indicator now shows at correct visual position
  - Tracks last visible view including indented folder contents
  - Shows at actual bottom when dragging past all items
- **Background Tab View Flash** - Browser view no longer shows new page when opening in background
  - CEF creates browser views visible by default
  - Now immediately hides browser view if tab is not active
- **Middle-Click on Links** - Fixed middle-click on web page links opening in current tab
  - `OnBeforePopup` now checks `target_disposition` for `CEF_WOD_NEW_BACKGROUND_TAB`
  - Properly routes to background tab creation instead of foreground
- **Bookmarks Page** (`orbfox://bookmarks`) - Native bookmarks page as default homepage
  - Grid layout with bookmark cards showing favicons and titles
  - Folder sections with collapsible organization
  - Dark theme matching OrbFox design system
  - Opens by default for new tabs and homepage
- **Homepage Quick Options** - Settings page now has quick buttons for homepage selection
  - "Bookmarks" button sets homepage/new tab to `orbfox://bookmarks`
  - "Blank" button sets to `about:blank`
  - Visual indicator shows currently selected option

### Fixed
- **New Tab Crash** - Fixed crash when opening new tabs
  - Dangling reference bug in address change callback
  - URL string now properly copied before async dispatch to main thread
  - Same fix applied to popup request callback
- **Autocomplete Selection** - URL bar now uses typed text on Enter, not auto-selected suggestion
  - No suggestion is selected by default when autocomplete appears
  - User must explicitly use arrow keys to select a suggestion
  - Enter key navigates to typed URL unless user selected a suggestion
- **Content Fullscreen Mode** - YouTube and HTML5 fullscreen now hides browser UI
  - Sidebar, toolbar, and address bar automatically hide when content goes fullscreen
  - Browser view expands to fill entire screen for true fullscreen experience
  - Press Escape to exit fullscreen - properly exits both browser and webpage fullscreen
  - UI elements restore when exiting fullscreen (via Escape or macOS window controls)
- **Screenshot in Fullscreen** - Screenshot shortcuts no longer exit fullscreen mode
  - Modifier-only key presses (Cmd, Shift, Ctrl, Option, Windows key) now filtered in fullscreen
  - Print Screen key also filtered on Windows
  - Prevents accidental fullscreen exit when taking screenshots (Cmd+Shift+4 on Mac, Print Screen on Windows)
- **Back/Forward Navigation** - Fixed Cmd+Shift accidentally triggering back navigation
  - Now uses native macOS key codes to correctly identify bracket keys
  - Back/Forward shortcuts require Cmd without Shift modifier
- **Context Menu Link Actions** - Fixed "Open Link in New Tab", "Open in Background Tab", and "Copy Link Address"
  - Fixed dangling reference bug where URL was captured by reference in async dispatch block
  - URL now properly copied before async dispatch to preserve value

### Added
- **Tab Drag & Drop Reordering** - Drag tabs to reorder them within a workspace
  - Visual drop indicator shows insertion point
  - Updates tab order in session storage
- **Workspace Drag & Drop Reordering** - Drag workspace tabs to reorder them
  - Horizontal drag between workspace tabs
  - Visual drop indicator shows insertion point

### Security
- **XSS Prevention in Error Pages** - HTML-escape untrusted content in 404 and error pages
  - Added `EscapeHtml()` utility function to escape `<`, `>`, `&`, `"`, `'`
  - Applied to error page URL and error message display
- **Path Traversal Prevention** - Validate download paths against directory traversal attacks
  - Added `IsValidDownloadPath()` utility function
  - Blocks paths with `..`, null bytes, and system directories (`/etc/`, `/usr/`, etc.)
  - Applied to both settings API and settings file loading
- **CORS/CSRF Protection** - Secure orbfox:// settings API
  - Removed dangerous `Access-Control-Allow-Origin: *` header
  - Added origin validation for POST requests (only accepts `orbfox://` origin)
  - Prevents malicious websites from modifying browser settings
- **Download Filename Sanitization** - Prevent path traversal via malicious filenames
  - Strip path components (`/`, `\`) from suggested download filenames
  - Remove `..` sequences to prevent directory traversal
  - Default to "download" if filename empty after sanitization
- **Remote Debugging Restricted** - Only enable DevTools remote debugging in debug builds
  - Port 9222 no longer exposed in release builds
- **URL Encoding for Error Pages** - Properly encode error page content in data: URLs
  - Added `UrlEncode()` utility function
  - Prevents issues with special characters in error messages

### Fixed
- **Loading Indicator Flickering** - Fixed "double load" appearance on JavaScript-heavy sites
  - Added debounced loading indicator following browser industry standards
  - 400ms delay before showing spinner (matches Safari/Chrome/Firefox behavior)
  - 200ms minimum display time once shown to prevent jarring flicker
  - Only shows for real navigations (OnLoadStart), ignores JavaScript-triggered loading states
  - Google, YouTube, and similar dynamic sites no longer trigger false loading indicators
- **Session Restore Infinite Loop** - Fixed hang on startup when restoring session
  - `DeleteWorkspace()` wouldn't delete the last workspace, causing infinite loop
  - Now creates restored workspaces first, then deletes default workspace
- **Tracking Protection Live Update** - Settings changes now take effect immediately
  - Previously required app restart for tracking protection toggle to work

### Changed
- **Atomic File Writes** - All persistent storage now uses atomic write pattern
  - Added `AtomicWriteFile()` - writes to `.tmp` then renames
  - Applied to settings, session, and window storage
  - Prevents corrupt files on crash during write
- **Thread-Safe Settings** - SettingsStorage now fully thread-safe
  - Added mutex protection for all read/write operations
  - `Get()` returns copy instead of reference for safety
- Total tests: 284 (was 219) - added settings storage tests, security validation tests

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
- **Find in Page** (Cmd+F) - Search for text within pages
  - Floating find bar at top-right of browser
  - Match highlighting on page
  - "X of Y" match count display
  - Previous/Next navigation (Enter, Shift+Enter, or arrow buttons)
  - Escape or × button to close
- **Settings Page** (orbfox://settings) - Browser settings UI
  - Custom protocol handler for `orbfox://` URLs
  - General: Homepage URL, New Tab URL, Session restore behavior
  - Privacy: Tracking protection toggle, Clear browsing data
  - Downloads: Default location, Ask before downloading
  - About: Version info
  - Settings persist across sessions in JSON format
  - Dark theme matching OrbFox design system
  - Gear icon displayed as tab favicon for orbfox:// pages
  - Settings icon in sidebar for quick access (opens in new tab)
  - Toggle switches with improved visibility (blue ON state, visible OFF state)
  - Settings pages recorded in browsing history
- **Clear History Button** - Clear all browsing history
  - "Clear" button in history panel title bar
  - Confirmation dialog before clearing
- **Reopen Closed Tab** (Cmd+Shift+T) - Restore recently closed tabs
  - Tracks up to 25 recently closed tabs
  - Reopens in original workspace (or active if deleted)
  - LIFO order (most recent first)
- **Move Tab Between Workspaces** - Move tabs to other workspaces
  - Right-click tab → "Move to" → select workspace
  - Shows all workspaces except current
  - Preserves tab properties (pinned, muted, title)
  - "New Space" option creates workspace and moves tab in one action
- **Link Context Menu** - Right-click links for quick actions
  - "Open Link in New Tab" - opens link in foreground tab
  - "Open Link in Background Tab" - opens link without switching
  - "Copy Link Address" - copies URL to clipboard
  - "Copy" appears for selected text
- **Unique Workspace Naming** - Auto-increment workspace names
  - New workspaces automatically named WS 1, WS 2, WS 3...
  - Skips existing numbers (e.g., if WS 2 exists, next is WS 3)
  - Duplicate names get suffix (e.g., "WS 1" → "WS 1 2")
- **Workspace Colors** - Color indicators on workspace tabs
  - 8-color palette: Blue, Red, Green, Orange, Purple, Pink, Teal, Yellow
  - Color dot shown on each workspace tab
  - Right-click → "Change Color" submenu with color swatches
  - Colors auto-assigned from palette on workspace creation
  - Colors persist across sessions
- **DevTools Panel** (Cmd+Opt+I) - Integrated Chrome DevTools
  - Slides in from right side as embedded panel (not separate window)
  - Native header bar with "DevTools" label and close button
  - Smooth slide-in/out animation with easing
  - Resizable via drag divider between browser and DevTools
  - "Inspect Element" from right-click menu opens at clicked position
  - Borderless styling integrated with main window
- **Workspace Switching Shortcuts** - Navigate between workspaces
  - Cmd+Opt+Left: Previous workspace (wraps around)
  - Cmd+Opt+Right: Next workspace (wraps around)
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
- **Workspace Selector Height** - Increased workspace area height for better tab visibility
- **Workspace Tab Alignment** - Tabs now properly aligned with plus button
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
- Total tests: 284 (was 219) - added settings storage tests (41 tests), security validation
- Removed redundant "Open in Space" from tab context menu (now use "Move to" with "New Space")

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
