#pragma once

#include <string>
#include <vector>
#include <ctime>
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
    bool Initialize();

    // Add a bookmark
    int64_t AddBookmark(const std::string& url, const std::string& title,
                        const std::string& folder = "");

    // Check if URL is bookmarked
    bool IsBookmarked(const std::string& url);

    // Get bookmark by URL (returns Bookmark with id=0 if not found)
    Bookmark GetBookmarkByUrl(const std::string& url);

    // Get all bookmarks
    std::vector<Bookmark> GetAllBookmarks();

    // Get bookmarks in a folder
    std::vector<Bookmark> GetBookmarksInFolder(const std::string& folder);

    // Get all folder names (including empty folders)
    std::vector<std::string> GetFolders();

    // Folder management
    bool CreateFolder(const std::string& name);
    bool FolderExists(const std::string& name);
    void DeleteFolder(const std::string& name);  // Also deletes bookmarks in folder
    void RenameFolder(const std::string& old_name, const std::string& new_name);
    int GetNextFolderNumber();  // For "Collection 1", "Collection 2", etc.

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
    static std::string GetDatabasePath();
    void CreateTables();

    void* db_ = nullptr;  // sqlite3*
};
