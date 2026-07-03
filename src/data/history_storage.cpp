#include "history_storage.h"

#include <sqlite3.h>

#include "utils/filesystem_utils.h"

HistoryStorage::HistoryStorage() = default;

HistoryStorage::~HistoryStorage() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::string HistoryStorage::GetDefaultDatabasePath() {
    return orbfox::utils::GetAppSupportPath() + "/history.db";
}

bool HistoryStorage::Initialize(const std::string& custom_path) {
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

void HistoryStorage::CreateTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL UNIQUE,
            title TEXT,
            visit_time INTEGER NOT NULL,
            visit_count INTEGER DEFAULT 1
        );
        CREATE INDEX IF NOT EXISTS idx_history_visit_time ON history(visit_time DESC);
        CREATE INDEX IF NOT EXISTS idx_history_url ON history(url);
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }
}

void HistoryStorage::AddEntry(const std::string& url, const std::string& title) {
    if (!db_ || url.empty()) return;

    // Insert or update existing entry
    const char* sql = R"(
        INSERT INTO history (url, title, visit_time, visit_count)
        VALUES (?, ?, ?, 1)
        ON CONFLICT(url) DO UPDATE SET
            title = excluded.title,
            visit_time = excluded.visit_time,
            visit_count = visit_count + 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, std::time(nullptr));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void HistoryStorage::ImportEntry(const std::string& url, const std::string& title,
                                 std::time_t visit_time, int visit_count) {
    if (!db_ || url.empty()) return;
    if (visit_time <= 0) visit_time = std::time(nullptr);
    if (visit_count < 1) visit_count = 1;

    // Preserve the original visit_time/visit_count; on conflict keep the MAX of each
    // so re-syncing the same data is idempotent (no drift, no lost recency/counts).
    const char* sql = R"(
        INSERT INTO history (url, title, visit_time, visit_count)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(url) DO UPDATE SET
            title = excluded.title,
            visit_time = MAX(visit_time, excluded.visit_time),
            visit_count = MAX(visit_count, excluded.visit_count)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, visit_time);
        sqlite3_bind_int(stmt, 4, visit_count);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<HistoryEntry> HistoryStorage::GetRecentHistory(int limit) {
    std::vector<HistoryEntry> entries;
    if (!db_) return entries;

    const char* sql = "SELECT id, url, title, visit_time, visit_count FROM history ORDER BY visit_time DESC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            entry.title = title ? title : "";
            entry.visit_time = sqlite3_column_int64(stmt, 3);
            entry.visit_count = sqlite3_column_int(stmt, 4);
            entries.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }

    return entries;
}

std::vector<HistoryEntry> HistoryStorage::GetAllHistory() {
    std::vector<HistoryEntry> entries;
    if (!db_) return entries;

    const char* sql = "SELECT id, url, title, visit_time, visit_count FROM history ORDER BY visit_time DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            entry.title = title ? title : "";
            entry.visit_time = sqlite3_column_int64(stmt, 3);
            entry.visit_count = sqlite3_column_int(stmt, 4);
            entries.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }

    return entries;
}

std::vector<HistoryEntry> HistoryStorage::SearchHistory(const std::string& query, int limit) {
    std::vector<HistoryEntry> entries;
    if (!db_ || query.empty()) return entries;

    const char* sql = R"(
        SELECT id, url, title, visit_time, visit_count FROM history
        WHERE url LIKE ? OR title LIKE ?
        ORDER BY visit_time DESC LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            entry.title = title ? title : "";
            entry.visit_time = sqlite3_column_int64(stmt, 3);
            entry.visit_count = sqlite3_column_int(stmt, 4);
            entries.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }

    return entries;
}

std::vector<HistoryEntry> HistoryStorage::GetHistoryForDay(std::time_t day_start) {
    std::vector<HistoryEntry> entries;
    if (!db_) return entries;

    std::time_t day_end = day_start + 86400;  // 24 hours

    const char* sql = R"(
        SELECT id, url, title, visit_time, visit_count FROM history
        WHERE visit_time >= ? AND visit_time < ?
        ORDER BY visit_time DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, day_start);
        sqlite3_bind_int64(stmt, 2, day_end);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            entry.title = title ? title : "";
            entry.visit_time = sqlite3_column_int64(stmt, 3);
            entry.visit_count = sqlite3_column_int(stmt, 4);
            entries.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }

    return entries;
}

void HistoryStorage::DeleteEntry(int64_t id) {
    if (!db_) return;

    const char* sql = "DELETE FROM history WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void HistoryStorage::ClearAllHistory() {
    if (!db_) return;

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM history", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }
}

void HistoryStorage::ClearHistoryBefore(std::time_t before_time) {
    if (!db_) return;

    const char* sql = "DELETE FROM history WHERE visit_time < ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, before_time);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
