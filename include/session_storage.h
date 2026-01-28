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
    bool HasSavedSession();

private:
    static std::string GetSessionPath();
};
