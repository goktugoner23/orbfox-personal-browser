#pragma once

#include <string>
#include <vector>
#include <ctime>

// Forward declaration for SQLite (avoids including sqlite3.h in header)
struct sqlite3;

// Represents a single history entry
struct HistoryEntry {
    int64_t id = 0;
    std::string url;
    std::string title;
    std::time_t visit_time = 0;
    int visit_count = 1;
};

// SQLite-based history storage
class HistoryStorage {
public:
    HistoryStorage();
    ~HistoryStorage();

    // Initialize database
    // If custom_path is empty, uses default production path
    // If custom_path is provided, uses that path (for testing)
    [[nodiscard]] bool Initialize(const std::string& custom_path = "");

    // Add a history entry (increments visit_count if URL exists)
    void AddEntry(const std::string& url, const std::string& title);

    // Query history
    std::vector<HistoryEntry> GetRecentHistory(int limit = 100);
    std::vector<HistoryEntry> SearchHistory(const std::string& query, int limit = 50);
    std::vector<HistoryEntry> GetHistoryForDay(std::time_t day_start);

    // Delete history
    void DeleteEntry(int64_t id);
    void ClearAllHistory();
    void ClearHistoryBefore(std::time_t before_time);

private:
    static std::string GetDefaultDatabasePath();
    void CreateTables();

    sqlite3* db_ = nullptr;
    std::string db_path_;  // Actual path used (empty until Initialize is called)
};

// Global accessor for the shared history storage instance
// Defined in browser_app.mm
HistoryStorage* GetHistoryStorage();
