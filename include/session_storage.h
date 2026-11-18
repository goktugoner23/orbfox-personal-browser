#pragma once

#include <string>
#include <vector>

// Represents a saved tab
struct SavedTab {
    std::string url;
    std::string title;
    bool is_pinned = false;
    bool is_muted = false;
};

// Represents a saved workspace
struct SavedWorkspace {
    std::string name;
    std::string color;
    std::vector<SavedTab> tabs;
    int active_tab_index = 0;
};

// Represents a saved session
struct SavedSession {
    std::vector<SavedWorkspace> workspaces;
    int active_workspace_index = 0;
};

// Handles saving and loading browser session state
class SessionStorage {
public:
    SessionStorage() = default;
    ~SessionStorage() = default;

    // Save session to disk
    void Save(const SavedSession& session);

    // Load session from disk
    SavedSession Load();

    // Parse a session from a JSON string (same format Save writes / the cloud stores).
    static SavedSession ParseJson(const std::string& json);

    // Three-way merge for cloud sync; base is the session as of the last successful sync.
    // A pin or workspace in base but gone on one side was closed there; an empty base only adds.
    static SavedSession Merge(const SavedSession& local, const SavedSession& remote,
                              const SavedSession& base);

    // The session last uploaded for this user, kept as the base for the next Merge.
    static void SaveSyncBase(const std::string& user_id, const std::string& session_json);
    static SavedSession LoadSyncBase(const std::string& user_id);

    // Raw JSON of the saved session, for uploading to cloud sync.
    // Returns "" if no session is saved.
    std::string ReadRawJson();

    // Check if a saved session exists
    [[nodiscard]] bool HasSavedSession();

    // Clear the saved session
    static void Clear();

    // Crash detection - uses a lock file to detect unclean shutdowns
    // Call MarkRunning() on startup, MarkCleanShutdown() on graceful exit
    static void MarkRunning();
    static void MarkCleanShutdown();
    [[nodiscard]] static bool DidCrashLastSession();

    // Crash reporting - writes crash info to a log file
    static void WriteCrashReport(const SavedSession& session);
    [[nodiscard]] static std::string GetLastCrashReport();
    static void ClearCrashReports();

    // Auto-save session periodically (call from timer)
    void AutoSave(const SavedSession& session);

    // Test-only path override to keep persistence tests out of app data.
    static void SetStorageDirectoryForTesting(const std::string& directory);
    static void ClearStorageDirectoryForTesting();

private:
    static std::string GetSessionPath();
    static std::string GetSyncBasePath(const std::string& user_id);
    static std::string GetCrashLockPath();
    static std::string GetCrashReportPath();
};
