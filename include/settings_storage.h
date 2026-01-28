#ifndef SETTINGS_STORAGE_H_
#define SETTINGS_STORAGE_H_

#include <string>

struct Settings {
    // General
    std::string homepage_url = "https://www.google.com";
    std::string new_tab_url = "https://www.google.com";
    bool restore_session = true;

    // Privacy
    bool tracking_protection = true;

    // Downloads
    std::string download_path = "";  // Empty = ~/Downloads
    bool ask_before_download = true;
};

class SettingsStorage {
public:
    static SettingsStorage& GetInstance();

    void Load();
    void Save();

    const Settings& Get() const { return settings_; }
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

    // Get resolved download path (expands ~ if needed)
    std::string GetResolvedDownloadPath() const;

    // Convert to/from JSON
    std::string ToJson() const;
    bool FromJson(const std::string& json);

private:
    SettingsStorage() = default;
    SettingsStorage(const SettingsStorage&) = delete;
    SettingsStorage& operator=(const SettingsStorage&) = delete;

    std::string GetSettingsPath() const;

    Settings settings_;
};

#endif  // SETTINGS_STORAGE_H_
