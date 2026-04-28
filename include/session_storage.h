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
    static std::string GetCrashLockPath();
    static std::string GetCrashReportPath();
};
