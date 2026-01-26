#pragma once

#include <string>
#include <vector>
#include <ctime>

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
    bool Initialize();

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
    static std::string GetDatabasePath();
    void CreateTables();

    void* db_ = nullptr;  // sqlite3*
};
