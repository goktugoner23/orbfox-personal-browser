#pragma once

#include <string>
#include <vector>
#include <functional>

// Supported browser types for import
enum class BrowserType {
    Chrome,
    Safari,
    Firefox,
    Edge
};

// Result of an import operation
struct ImportResult {
    bool success = false;
    int imported_count = 0;
    int skipped_count = 0;  // Duplicates or invalid
    std::string error_message;
};

// Detected browser with importable bookmarks
struct DetectedBrowser {
    BrowserType type;
    std::string name;           // Display name (e.g., "Google Chrome")
    std::string profile_path;   // Path to profile directory
    std::string profile_name;   // Profile name (e.g., "Default", "Profile 1")
};

// Bookmark importer - imports bookmarks from other browsers
class BookmarkImporter {
public:
    // Detect installed browsers with importable bookmarks
    static std::vector<DetectedBrowser> DetectBrowsers();

    // Import bookmarks from a specific browser/profile
    // folder_name: OrbFox folder to import into (empty = create folder named after browser)
    static ImportResult Import(const DetectedBrowser& browser, const std::string& folder_name = "");

    // Import from specific browser type (uses default profile)
    static ImportResult ImportFromChrome(const std::string& folder_name = "");
    static ImportResult ImportFromSafari(const std::string& folder_name = "");
    static ImportResult ImportFromFirefox(const std::string& folder_name = "");
    static ImportResult ImportFromEdge(const std::string& folder_name = "");

    // Get browser name for display
    static std::string GetBrowserName(BrowserType type);

private:
    // Path helpers
    static std::string GetChromePath();
    static std::string GetSafariPath();
    static std::string GetFirefoxProfilesPath();
    static std::string GetEdgePath();

    // Parsers
    static ImportResult ParseChromeBookmarks(const std::string& path, const std::string& folder_name);
    static ImportResult ParseSafariBookmarks(const std::string& path, const std::string& folder_name);
    static ImportResult ParseFirefoxBookmarks(const std::string& path, const std::string& folder_name);
};
