#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <ctime>
#include <optional>

// Download state enum
enum class DownloadState {
    InProgress,
    Complete,
    Canceled,
    Interrupted,
    Paused
};

// Download item structure
struct DownloadItem {
    uint32_t id = 0;
    std::string url;           // Final URL after redirects (from CEF)
    std::string original_url;  // Original URL before redirects (for display/restart)
    std::string filename;
    std::string full_path;
    std::string mime_type;
    int64_t total_bytes = 0;
    int64_t received_bytes = 0;
    int percent_complete = 0;
    int64_t current_speed = 0;  // bytes/sec
    DownloadState state = DownloadState::InProgress;
    std::time_t start_time = 0;
    std::time_t end_time = 0;
};

// Callback types
using DownloadUpdateCallback = std::function<void()>;
using DownloadCancelCallback = std::function<void()>;

// Download manager - singleton to track all downloads
class DownloadManager {
public:
    static DownloadManager& GetInstance();

    // Add or update a download
    void UpdateDownload(const DownloadItem& item);

    // Get all downloads (most recent first)
    std::vector<DownloadItem> GetDownloads() const;

    // Get download by ID (returns copy for thread safety)
    std::optional<DownloadItem> GetDownload(uint32_t id);

    // Set cancel callback for a download
    void SetCancelCallback(uint32_t id, DownloadCancelCallback callback);
    void RemoveCancelCallback(uint32_t id);

    // Cancel a download
    void CancelDownload(uint32_t id);

    // Pause/resume a download
    void PauseDownload(uint32_t id);
    void ResumeDownload(uint32_t id);

    // Clear completed/canceled downloads
    void ClearCompleted();

    // Remove a specific download from the list
    void RemoveDownload(uint32_t id);

    // Persistence
    // If custom_path is empty, uses default production path
    // If custom_path is provided, uses that path (for testing)
    void LoadFromDisk(const std::string& custom_path = "");
    void SaveToDisk(const std::string& custom_path = "");

    // Set UI update callback (called when downloads change)
    void SetUpdateCallback(DownloadUpdateCallback callback);

    // Get overall progress of active downloads (0.0 - 1.0, or -1 if no active downloads)
    float GetOverallProgress() const;

    // Check if any downloads are in progress
    bool HasActiveDownloads() const;

    // Set pending original URL for next download (used when restarting)
    void SetPendingOriginalUrl(const std::string& url);

    // Get and clear pending original URL
    std::string GetAndClearPendingOriginalUrl();

    // Mark next download as a restart (to apply saved preferences)
    void SetIsRestart(bool is_restart);
    bool GetAndClearIsRestart();

    // Reset all state (for testing only - clears downloads, callbacks, and flags)
    void ResetForTesting();

private:
    DownloadManager() = default;
    ~DownloadManager() = default;
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    mutable std::mutex mutex_;
    std::vector<DownloadItem> downloads_;
    std::map<uint32_t, DownloadCancelCallback> cancel_callbacks_;
    // Downloads canceled before their cancel callback was registered (race: user hit Cancel
    // before OnDownloadUpdated fired). Canceled as soon as the callback shows up.
    std::set<uint32_t> pending_cancels_;
    DownloadUpdateCallback on_update_;
    std::string pending_original_url_;
    bool is_restart_ = false;
};
