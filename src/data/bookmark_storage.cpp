#include "bookmark_storage.h"

#include <sqlite3.h>
#include <cstdlib>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void EnsureDirectoryExists(const std::string& path) {
#ifdef __APPLE__
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
#endif
}

}  // namespace

BookmarkStorage::BookmarkStorage() = default;

BookmarkStorage::~BookmarkStorage() {
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
}

std::string BookmarkStorage::GetDatabasePath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    std::string app_support = std::string(home) + "/Library/Application Support/OrbFox";
    EnsureDirectoryExists(app_support);
    return app_support + "/bookmarks.db";
#else
    return "bookmarks.db";
#endif
}

bool BookmarkStorage::Initialize() {
    std::string path = GetDatabasePath();

    int rc = sqlite3_open(path.c_str(), reinterpret_cast<sqlite3**>(&db_));
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
    )";

    sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, nullptr);
}

int64_t BookmarkStorage::AddBookmark(const std::string& url, const std::string& title,
                                      const std::string& folder) {
    if (!db_ || url.empty()) return 0;

    // Get max position in folder for ordering
    int max_position = 0;
    {
        const char* pos_sql = "SELECT MAX(position) FROM bookmarks WHERE folder = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, folder.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, std::time(nullptr));
        sqlite3_bind_int(stmt, 5, max_position + 1);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            bookmark_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }

    return found;
}

Bookmark BookmarkStorage::GetBookmarkByUrl(const std::string& url) {
    Bookmark bookmark;
    if (!db_ || url.empty()) return bookmark;

    const char* sql = "SELECT id, url, title, folder, created_time, position FROM bookmarks WHERE url = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            bookmark.id = sqlite3_column_int64(stmt, 0);
            bookmark.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            bookmark.title = title ? title : "";
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            bookmark.folder = folder ? folder : "";
            bookmark.created_time = sqlite3_column_int64(stmt, 4);
            bookmark.position = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    return bookmark;
}

std::vector<Bookmark> BookmarkStorage::GetAllBookmarks() {
    std::vector<Bookmark> bookmarks;
    if (!db_) return bookmarks;

    const char* sql = "SELECT id, url, title, folder, created_time, position FROM bookmarks ORDER BY folder, position";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Bookmark bookmark;
            bookmark.id = sqlite3_column_int64(stmt, 0);
            bookmark.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, folder.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Bookmark bookmark;
            bookmark.id = sqlite3_column_int64(stmt, 0);
            bookmark.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
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

    const char* sql = "SELECT DISTINCT folder FROM bookmarks WHERE folder != '' ORDER BY folder";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::DeleteBookmarkByUrl(const std::string& url) {
    if (!db_ || url.empty()) return;

    const char* sql = "DELETE FROM bookmarks WHERE url = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void BookmarkStorage::MoveBookmark(int64_t id, const std::string& new_folder, int new_position) {
    if (!db_) return;

    const char* sql = "UPDATE bookmarks SET folder = ?, position = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_folder.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, new_position);
        sqlite3_bind_int64(stmt, 3, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
