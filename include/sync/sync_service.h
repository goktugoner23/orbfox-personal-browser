#pragma once

#include "sync_types.h"
#include <string>
#include <functional>
#include <ctime>

// Sync result
struct SyncResult {
    bool success = false;
    std::string error_message;
    int bookmarks_synced = 0;
    int history_synced = 0;
    bool settings_synced = false;
};

using SyncCallback = std::function<void(const SyncResult&)>;

// Cloud sync service using Firebase Realtime Database
class SyncService {
public:
    static SyncService& GetInstance();

    // Perform full sync (upload local data, then download and merge remote)
    void SyncNow(SyncCallback callback);

    // Upload only (push local data to cloud)
    void Upload(SyncCallback callback);

    // Download only (pull remote data and merge)
    void Download(SyncCallback callback);

    // Get last sync time
    std::time_t GetLastSyncTime() const { return last_sync_time_; }

    // Set Firebase database URL (must be called before syncing)
    void SetFirebaseUrl(const std::string& url) { firebase_url_ = url; }

private:
    SyncService() = default;
    SyncService(const SyncService&) = delete;
    SyncService& operator=(const SyncService&) = delete;

    // Export local data to JSON
    std::string ExportBookmarksToJson();
    std::string ExportHistoryToJson();
    std::string ExportSettingsToJson();

    // Import data from JSON
    bool ImportBookmarksFromJson(const std::string& json);
    bool ImportHistoryFromJson(const std::string& json);
    bool ImportSettingsFromJson(const std::string& json);

    // Firebase REST API helpers
    std::string HttpPut(const std::string& url, const std::string& body, const std::string& token);
    std::string HttpGet(const std::string& url, const std::string& token);

    std::string firebase_url_;
    std::time_t last_sync_time_ = 0;
};
