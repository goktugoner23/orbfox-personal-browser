#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <optional>

// Forward declaration for SQLite (avoids including sqlite3.h in header)
struct sqlite3;

// Represents a single bookmark
struct Bookmark {
    int64_t id = 0;
    std::string url;
    std::string title;
    std::string folder;  // Empty for root level
    std::time_t created_time = 0;
    int position = 0;  // For ordering within folder
};

// SQLite-based bookmark storage
class BookmarkStorage {
public:
    BookmarkStorage();
    ~BookmarkStorage();

    // Initialize database
    // If custom_path is empty, uses default production path
    // If custom_path is provided, uses that path (for testing)
    [[nodiscard]] bool Initialize(const std::string& custom_path = "");

    // Add a bookmark
    [[nodiscard]] int64_t AddBookmark(const std::string& url, const std::string& title,
                                      const std::string& folder = "");

    // Check if URL is bookmarked
    [[nodiscard]] bool IsBookmarked(const std::string& url);

    // Get bookmark by URL (returns nullopt if not found)
    [[nodiscard]] std::optional<Bookmark> GetBookmarkByUrl(const std::string& url);

    // Get all bookmarks
    std::vector<Bookmark> GetAllBookmarks();

    // Get bookmarks in a folder
    std::vector<Bookmark> GetBookmarksInFolder(const std::string& folder);

    // Get all folder names (including empty folders)
    std::vector<std::string> GetFolders();

    // Folder management
    [[nodiscard]] bool CreateFolder(const std::string& name);
    [[nodiscard]] bool FolderExists(const std::string& name);
    void DeleteFolder(const std::string& name);  // Also deletes bookmarks in folder
    void RenameFolder(const std::string& old_name, const std::string& new_name);
    [[nodiscard]] int GetNextFolderNumber();  // For "Collection 1", "Collection 2", etc.

    // Update bookmark
    void UpdateBookmark(int64_t id, const std::string& title, const std::string& folder);

    // Delete bookmark
    void DeleteBookmark(int64_t id);
    void DeleteBookmarkByUrl(const std::string& url);

    // Reorder bookmarks
    void MoveBookmark(int64_t id, const std::string& new_folder, int new_position);

    // Clear all bookmarks (for testing)
    void ClearAllBookmarks();

private:
    static std::string GetDefaultDatabasePath();
    void CreateTables();

    sqlite3* db_ = nullptr;
    std::string db_path_;  // Actual path used (empty until Initialize is called)
};

// Global accessor for the shared bookmark storage instance
// Defined in browser_app.mm
BookmarkStorage* GetBookmarkStorage();
