#include "history_storage.h"

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

HistoryStorage::HistoryStorage() = default;

HistoryStorage::~HistoryStorage() {
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
}

std::string HistoryStorage::GetDatabasePath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    std::string app_support = std::string(home) + "/Library/Application Support/OrbFox";
    EnsureDirectoryExists(app_support);
    return app_support + "/history.db";
#else
    return "history.db";
#endif
}

bool HistoryStorage::Initialize() {
    std::string path = GetDatabasePath();

    int rc = sqlite3_open(path.c_str(), reinterpret_cast<sqlite3**>(&db_));
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

    sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, nullptr);
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, std::time(nullptr));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<HistoryEntry> HistoryStorage::GetRecentHistory(int limit) {
    std::vector<HistoryEntry> entries;
    if (!db_) return entries;

    const char* sql = "SELECT id, url, title, visit_time, visit_count FROM history ORDER BY visit_time DESC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, day_start);
        sqlite3_bind_int64(stmt, 2, day_end);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry entry;
            entry.id = sqlite3_column_int64(stmt, 0);
            entry.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
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
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void HistoryStorage::ClearAllHistory() {
    if (!db_) return;
    sqlite3_exec(static_cast<sqlite3*>(db_), "DELETE FROM history", nullptr, nullptr, nullptr);
}

void HistoryStorage::ClearHistoryBefore(std::time_t before_time) {
    if (!db_) return;

    const char* sql = "DELETE FROM history WHERE visit_time < ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, before_time);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
