#pragma once

#include <mutex>
#include <string>
#include <vector>

struct SearchShortcut {
    std::string key;           // e.g. "y"
    std::string url_template;  // e.g. "https://www.youtube.com/results?search_query=%s"
};

struct Settings {
    // General
    std::string homepage_url = "orbfox://bookmarks";
    std::string new_tab_url = "orbfox://bookmarks";
    bool restore_session = true;

    // Privacy
    bool tracking_protection = true;

    // Downloads
    std::string download_path = "";  // Empty = ~/Downloads
    bool ask_before_download = true;

    // Gestures
    bool gestures_enabled = true;
    bool gesture_back_enabled = true;       // Left drag = go back
    bool gesture_forward_enabled = true;    // Right drag = go forward
    bool gesture_close_tab_enabled = true;  // L-shape = close tab
    bool gesture_reopen_tab_enabled = true; // Reverse L-shape = reopen closed tab

    // Search shortcuts (Vivaldi-style)
    std::vector<SearchShortcut> search_shortcuts = {
        {"g", "https://www.google.com/search?q=%s"},
        {"y", "https://www.youtube.com/results?search_query=%s"},
        {"a", "https://www.amazon.com/s?k=%s"},
    };
};

class SettingsStorage {
public:
    static SettingsStorage& GetInstance();

    void Load();
    void Save();

    Settings Get() const;  // Returns a copy for thread safety
    void Set(const Settings& settings);

    // Individual setters
    void SetHomepage(const std::string& url);
    void SetNewTabUrl(const std::string& url);
    void SetRestoreSession(bool restore);
    void SetTrackingProtection(bool enabled);
    void SetDownloadPath(const std::string& path);
    void SetAskBeforeDownload(bool ask);

    // Search is always Google
    static std::string GetSearchUrl(const std::string& query);

    // Resolve address bar input: scheme check → shortcut → URL-like → search fallback
    std::string ResolveAddressBarInput(const std::string& input) const;

    // Get resolved download path (expands ~ if needed)
    std::string GetResolvedDownloadPath() const;

    // Convert to/from JSON
    std::string ToJson() const;
    [[nodiscard]] bool FromJson(const std::string& json);

private:
    SettingsStorage() = default;
    SettingsStorage(const SettingsStorage&) = delete;
    SettingsStorage& operator=(const SettingsStorage&) = delete;

    std::string GetSettingsPath() const;
    std::string ToJsonLocked() const;
    [[nodiscard]] bool FromJsonLocked(const std::string& json);
    void SaveLocked() const;

    mutable std::mutex mutex_;
    Settings settings_;
};
