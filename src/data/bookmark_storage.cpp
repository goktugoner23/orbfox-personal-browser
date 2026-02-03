#include "bookmark_storage.h"

#include <sqlite3.h>

#include "utils/filesystem_utils.h"

BookmarkStorage::BookmarkStorage() = default;

BookmarkStorage::~BookmarkStorage() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::string BookmarkStorage::GetDefaultDatabasePath() {
    return orbfox::utils::GetAppSupportPath() + "/bookmarks.db";
}

bool BookmarkStorage::Initialize(const std::string& custom_path) {
    // Use custom path if provided, otherwise use default production path
    db_path_ = custom_path.empty() ? GetDefaultDatabasePath() : custom_path;

    // Ensure parent directory exists for custom paths
    if (!custom_path.empty()) {
        auto last_slash = custom_path.rfind('/');
        if (last_slash != std::string::npos) {
            std::string dir = custom_path.substr(0, last_slash);
            orbfox::utils::EnsureDirectoryExists(dir);
        }
    }

    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        return false;
    }

    CreateTables();
    return true;
}

void BookmarkStorage::CreateTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS bookmarks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL UNIQUE,
            title TEXT,
            folder TEXT DEFAULT '',
            created_time INTEGER NOT NULL,
            position INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_bookmarks_url ON bookmarks(url);
        CREATE INDEX IF NOT EXISTS idx_bookmarks_folder ON bookmarks(folder);

        CREATE TABLE IF NOT EXISTS bookmark_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            created_time INTEGER NOT NULL,
            position INTEGER DEFAULT 0
        );
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }
}

int64_t BookmarkStorage::AddBookmark(const std::string& url, const std::string& title,
                                      const std::string& folder) {
    if (!db_ || url.empty()) return 0;

    // Get max position in folder for ordering
    int max_position = 0;
    {
        const char* pos_sql = "SELECT MAX(position) FROM bookmarks WHERE folder = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, folder.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                max_position = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    const char* sql = R"(
        INSERT INTO bookmarks (url, title, folder, created_time, position)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(url) DO UPDATE SET
            title = excluded.title,
            folder = excluded.folder
    )";

    sqlite3_stmt* stmt = nullptr;
    int64_t bookmark_id = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, folder.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int(stmt, 5, max_position + 1);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            bookmark_id = sqlite3_last_insert_rowid(db_);
        }
        sqlite3_finalize(stmt);
    }

    return bookmark_id;
}

bool BookmarkStorage::IsBookmarked(const std::string& url) {
    if (!db_ || url.empty()) return false;

    const char* sql = "SELECT COUNT(*) FROM bookmarks WHERE url = ?";

    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    return found;
}

std::optional<Bookmark> BookmarkStorage::GetBookmarkByUrl(const std::string& url) {
    if (!db_ || url.empty()) return std::nullopt;

    const char* sql = "SELECT id, url, title, folder, created_time, position FROM bookmarks WHERE url = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Bookmark bookmark;
            bookmark.id = sqlite3_column_int64(stmt, 0);
            const char* url_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            bookmark.url = url_text ? url_text : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            bookmark.title = title ? title : "";
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            bookmark.folder = folder ? folder : "";
            bookmark.created_time = sqlite3_column_int64(stmt, 4);
            bookmark.position = sqlite3_column_int(stmt, 5);
            sqlite3_finalize(stmt);
            return bookmark;
        }
        sqlite3_finalize(stmt);
    }

    return std::nullopt;
}

std::vector<Bookmark> BookmarkStorage::GetAllBookmarks() {
    std::vector<Bookmark> bookmarks;
    if (!db_) return bookmarks;

    const char* sql = "SELECT id, url, title, folder, created_time, position FROM bookmarks ORDER BY folder, position";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Bookmark bookmark;
            bookmark.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            bookmark.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            bookmark.title = title ? title : "";
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            bookmark.folder = folder ? folder : "";
            bookmark.created_time = sqlite3_column_int64(stmt, 4);
            bookmark.position = sqlite3_column_int(stmt, 5);
            bookmarks.push_back(bookmark);
        }
        sqlite3_finalize(stmt);
    }

    return bookmarks;
}

std::vector<Bookmark> BookmarkStorage::GetBookmarksInFolder(const std::string& folder) {
    std::vector<Bookmark> bookmarks;
    if (!db_) return bookmarks;

    const char* sql = "SELECT id, url, title, folder, created_time, position FROM bookmarks WHERE folder = ? ORDER BY position";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, folder.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Bookmark bookmark;
            bookmark.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            bookmark.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            bookmark.title = title ? title : "";
            const char* folder_val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            bookmark.folder = folder_val ? folder_val : "";
            bookmark.created_time = sqlite3_column_int64(stmt, 4);
            bookmark.position = sqlite3_column_int(stmt, 5);
            bookmarks.push_back(bookmark);
        }
        sqlite3_finalize(stmt);
    }

    return bookmarks;
}

std::vector<std::string> BookmarkStorage::GetFolders() {
    std::vector<std::string> folders;
    if (!db_) return folders;

    // Get folders from bookmark_folders table (includes empty folders)
    const char* sql1 = "SELECT name FROM bookmark_folders ORDER BY position, name";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (folder) {
                folders.push_back(folder);
            }
        }
        sqlite3_finalize(stmt);
    }

    // Also get folders from bookmarks that aren't in the folders table
    const char* sql2 = R"(
        SELECT DISTINCT folder FROM bookmarks
        WHERE folder != '' AND folder NOT IN (SELECT name FROM bookmark_folders)
        ORDER BY folder
    )";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (folder) {
                folders.push_back(folder);
            }
        }
        sqlite3_finalize(stmt);
    }

    return folders;
}

void BookmarkStorage::UpdateBookmark(int64_t id, const std::string& title, const std::string& folder) {
    if (!db_) return;

    const char* sql = "UPDATE bookmarks SET title = ?, folder = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, folder.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::DeleteBookmark(int64_t id) {
    if (!db_) return;

    const char* sql = "DELETE FROM bookmarks WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::DeleteBookmarkByUrl(const std::string& url) {
    if (!db_ || url.empty()) return;

    const char* sql = "DELETE FROM bookmarks WHERE url = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::MoveBookmark(int64_t id, const std::string& new_folder, int new_position) {
    if (!db_) return;

    const char* sql = "UPDATE bookmarks SET folder = ?, position = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_folder.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, new_position);
        sqlite3_bind_int64(stmt, 3, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::ClearAllBookmarks() {
    if (!db_) return;

    char* err_msg = nullptr;

    const char* sql = "DELETE FROM bookmarks";
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
            err_msg = nullptr;
        }
    }

    const char* sql2 = "DELETE FROM bookmark_folders";
    rc = sqlite3_exec(db_, sql2, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }
}

bool BookmarkStorage::CreateFolder(const std::string& name) {
    if (!db_ || name.empty()) return false;

    // Get max position for ordering
    int max_position = 0;
    {
        const char* pos_sql = "SELECT MAX(position) FROM bookmark_folders";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                max_position = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    const char* sql = "INSERT OR IGNORE INTO bookmark_folders (name, created_time, position) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, std::time(nullptr));
        sqlite3_bind_int(stmt, 3, max_position + 1);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = sqlite3_changes(db_) > 0;
        }
        sqlite3_finalize(stmt);
    }

    return success;
}

bool BookmarkStorage::FolderExists(const std::string& name) {
    if (!db_ || name.empty()) return false;

    // Check in bookmark_folders table
    const char* sql1 = "SELECT COUNT(*) FROM bookmark_folders WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_int(stmt, 0) > 0) {
                sqlite3_finalize(stmt);
                return true;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Also check in bookmarks table (folders created implicitly)
    const char* sql2 = "SELECT COUNT(*) FROM bookmarks WHERE folder = ?";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            bool exists = sqlite3_column_int(stmt, 0) > 0;
            sqlite3_finalize(stmt);
            return exists;
        }
        sqlite3_finalize(stmt);
    }

    return false;
}

void BookmarkStorage::DeleteFolder(const std::string& name) {
    if (!db_ || name.empty()) return;

    // Delete all bookmarks in the folder
    const char* sql1 = "DELETE FROM bookmarks WHERE folder = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Delete from bookmark_folders table
    const char* sql2 = "DELETE FROM bookmark_folders WHERE name = ?";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::RenameFolder(const std::string& old_name, const std::string& new_name) {
    if (!db_ || old_name.empty() || new_name.empty()) return;

    // Update bookmarks
    const char* sql1 = "UPDATE bookmarks SET folder = ? WHERE folder = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, old_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Update bookmark_folders table
    const char* sql2 = "UPDATE bookmark_folders SET name = ? WHERE name = ?";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, old_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::MoveFolder(const std::string& name, int new_position) {
    if (!db_ || name.empty()) return;

    // Get current position
    int current_position = -1;
    const char* get_pos_sql = "SELECT position FROM bookmark_folders WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, get_pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_position = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (current_position < 0) return;  // Folder not found

    // Shift other folders
    if (new_position < current_position) {
        // Moving up: shift folders in range [new_position, current_position) down
        const char* shift_sql = "UPDATE bookmark_folders SET position = position + 1 WHERE position >= ? AND position < ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, new_position);
            sqlite3_bind_int(stmt, 2, current_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (new_position > current_position) {
        // Moving down: shift folders in range (current_position, new_position] up
        const char* shift_sql = "UPDATE bookmark_folders SET position = position - 1 WHERE position > ? AND position <= ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_position);
            sqlite3_bind_int(stmt, 2, new_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Update this folder's position
    const char* update_sql = "UPDATE bookmark_folders SET position = ? WHERE name = ?";
    if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, new_position);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

int BookmarkStorage::GetNextFolderNumber() {
    if (!db_) return 1;

    int max_num = 0;

    // Check bookmark_folders table for "Collection N" pattern
    const char* sql1 = "SELECT name FROM bookmark_folders WHERE name LIKE 'Collection %'";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (name) {
                int num = 0;
                if (sscanf(name, "Collection %d", &num) == 1 && num > max_num) {
                    max_num = num;
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    // Also check bookmarks table (implicit folders)
    const char* sql2 = "SELECT DISTINCT folder FROM bookmarks WHERE folder LIKE 'Collection %'";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (name) {
                int num = 0;
                if (sscanf(name, "Collection %d", &num) == 1 && num > max_num) {
                    max_num = num;
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    return max_num + 1;
}

int BookmarkStorage::GetFolderPosition(const std::string& name) {
    if (!db_ || name.empty()) return -1;

    const char* sql = "SELECT position FROM bookmark_folders WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    int position = -1;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            position = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return position;
}

void BookmarkStorage::MoveBookmarkAtRoot(int64_t id, int new_position) {
    if (!db_) return;

    // Get current position of this bookmark (must be at root level - empty folder)
    int current_position = -1;
    const char* get_pos_sql = "SELECT position FROM bookmarks WHERE id = ? AND folder = ''";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, get_pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_position = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (current_position < 0) return;  // Bookmark not found at root

    // Shift other root bookmarks
    if (new_position < current_position) {
        const char* shift_sql = "UPDATE bookmarks SET position = position + 1 WHERE folder = '' AND position >= ? AND position < ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, new_position);
            sqlite3_bind_int(stmt, 2, current_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (new_position > current_position) {
        const char* shift_sql = "UPDATE bookmarks SET position = position - 1 WHERE folder = '' AND position > ? AND position <= ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_position);
            sqlite3_bind_int(stmt, 2, new_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Also shift folders in the same range
    if (new_position < current_position) {
        const char* shift_sql = "UPDATE bookmark_folders SET position = position + 1 WHERE position >= ? AND position < ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, new_position);
            sqlite3_bind_int(stmt, 2, current_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (new_position > current_position) {
        const char* shift_sql = "UPDATE bookmark_folders SET position = position - 1 WHERE position > ? AND position <= ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_position);
            sqlite3_bind_int(stmt, 2, new_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Update this bookmark's position
    const char* update_sql = "UPDATE bookmarks SET position = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, new_position);
        sqlite3_bind_int64(stmt, 2, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::MoveFolderAtRoot(const std::string& name, int new_position) {
    if (!db_ || name.empty()) return;

    // Get current position
    int current_position = GetFolderPosition(name);
    if (current_position < 0) return;

    sqlite3_stmt* stmt = nullptr;

    // Shift other folders
    if (new_position < current_position) {
        const char* shift_sql = "UPDATE bookmark_folders SET position = position + 1 WHERE position >= ? AND position < ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, new_position);
            sqlite3_bind_int(stmt, 2, current_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (new_position > current_position) {
        const char* shift_sql = "UPDATE bookmark_folders SET position = position - 1 WHERE position > ? AND position <= ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_position);
            sqlite3_bind_int(stmt, 2, new_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Also shift root bookmarks in the same range
    if (new_position < current_position) {
        const char* shift_sql = "UPDATE bookmarks SET position = position + 1 WHERE folder = '' AND position >= ? AND position < ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, new_position);
            sqlite3_bind_int(stmt, 2, current_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (new_position > current_position) {
        const char* shift_sql = "UPDATE bookmarks SET position = position - 1 WHERE folder = '' AND position > ? AND position <= ?";
        if (sqlite3_prepare_v2(db_, shift_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_position);
            sqlite3_bind_int(stmt, 2, new_position);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Update this folder's position
    const char* update_sql = "UPDATE bookmark_folders SET position = ? WHERE name = ?";
    if (sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, new_position);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
