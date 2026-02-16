#include "settings_storage.h"

#include <fstream>
#include <sstream>

#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"

namespace {

std::string ExpandTilde(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    return orbfox::utils::GetHomeDirectory() + path.substr(1);
}

}  // namespace

SettingsStorage& SettingsStorage::GetInstance() {
    static SettingsStorage instance;
    return instance;
}

std::string SettingsStorage::GetSettingsPath() const {
    return orbfox::utils::GetAppSupportPath() + "/settings.json";
}

void SettingsStorage::Load() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(GetSettingsPath());
    if (!file.is_open()) {
        return;  // Use defaults
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Intentionally ignore return - Load() silently uses defaults on parse failure
    (void)FromJson(json);
}

void SettingsStorage::Save() {
    // Note: caller must hold mutex_
    // Intentionally ignore return - Save() is best-effort and callers don't expect errors
    (void)orbfox::utils::AtomicWriteFile(GetSettingsPath(), ToJson());
}

Settings SettingsStorage::Get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;  // Return a copy
}

void SettingsStorage::Set(const Settings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    Save();
}

void SettingsStorage::SetHomepage(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.homepage_url = url;
    Save();
}

void SettingsStorage::SetNewTabUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.new_tab_url = url;
    Save();
}

void SettingsStorage::SetRestoreSession(bool restore) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.restore_session = restore;
    Save();
}

void SettingsStorage::SetTrackingProtection(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.tracking_protection = enabled;
    Save();
}

void SettingsStorage::SetDownloadPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate path to prevent path traversal attacks
    if (!path.empty() && !orbfox::utils::IsValidDownloadPath(path)) {
        return;  // Silently reject invalid paths
    }
    settings_.download_path = path;
    Save();
}

void SettingsStorage::SetAskBeforeDownload(bool ask) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.ask_before_download = ask;
    Save();
}

namespace {

std::string UrlEncode(const std::string& str) {
    std::string encoded;
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            encoded += hex;
        }
    }
    return encoded;
}

}  // namespace

std::string SettingsStorage::GetSearchUrl(const std::string& query) {
    return "https://www.google.com/search?q=" + UrlEncode(query);
}

std::string SettingsStorage::ResolveAddressBarInput(const std::string& input) const {
    if (input.empty()) return input;

    // 1. Has scheme? Return as-is
    if (input.find("://") != std::string::npos) {
        return input;
    }

    // Also handle orbfox: scheme without //
    if (input.find("orbfox:") == 0) {
        return input;
    }

    // 2. Check for search shortcut: split on first space
    size_t space_pos = input.find(' ');
    if (space_pos != std::string::npos) {
        std::string prefix = input.substr(0, space_pos);
        std::string query = input.substr(space_pos + 1);

        // Copy shortcuts under lock to avoid holding mutex during URL encoding
        std::vector<SearchShortcut> shortcuts;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shortcuts = settings_.search_shortcuts;
        }
        for (const auto& shortcut : shortcuts) {
            if (shortcut.key == prefix) {
                std::string url = shortcut.url_template;
                size_t pos = url.find("%s");
                if (pos != std::string::npos) {
                    url.replace(pos, 2, UrlEncode(query));
                }
                return url;
            }
        }
    }

    // 3. URL-like? (has dot + no space)
    if (input.find('.') != std::string::npos && input.find(' ') == std::string::npos) {
        return "https://" + input;
    }

    // 4. Fallback to Google search
    return GetSearchUrl(input);
}

std::string SettingsStorage::GetResolvedDownloadPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (settings_.download_path.empty()) {
        return orbfox::utils::GetHomeDirectory() + "/Downloads";
    }
    return ExpandTilde(settings_.download_path);
}

std::string SettingsStorage::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"homepage_url\": \"" << orbfox::utils::EscapeJsonString(settings_.homepage_url) << "\",\n";
    ss << "  \"new_tab_url\": \"" << orbfox::utils::EscapeJsonString(settings_.new_tab_url) << "\",\n";
    ss << "  \"restore_session\": " << (settings_.restore_session ? "true" : "false") << ",\n";
    ss << "  \"tracking_protection\": " << (settings_.tracking_protection ? "true" : "false") << ",\n";
    ss << "  \"download_path\": \"" << orbfox::utils::EscapeJsonString(settings_.download_path) << "\",\n";
    ss << "  \"ask_before_download\": " << (settings_.ask_before_download ? "true" : "false") << ",\n";
    ss << "  \"gestures_enabled\": " << (settings_.gestures_enabled ? "true" : "false") << ",\n";
    ss << "  \"gesture_back_enabled\": " << (settings_.gesture_back_enabled ? "true" : "false") << ",\n";
    ss << "  \"gesture_forward_enabled\": " << (settings_.gesture_forward_enabled ? "true" : "false") << ",\n";
    ss << "  \"gesture_close_tab_enabled\": " << (settings_.gesture_close_tab_enabled ? "true" : "false") << ",\n";
    ss << "  \"gesture_reopen_tab_enabled\": " << (settings_.gesture_reopen_tab_enabled ? "true" : "false") << ",\n";
    ss << "  \"search_shortcuts\": [";
    for (size_t i = 0; i < settings_.search_shortcuts.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\n    {\"key\": \"" << orbfox::utils::EscapeJsonString(settings_.search_shortcuts[i].key)
           << "\", \"url_template\": \"" << orbfox::utils::EscapeJsonString(settings_.search_shortcuts[i].url_template) << "\"}";
    }
    ss << "\n  ]\n";
    ss << "}\n";
    return ss.str();
}

bool SettingsStorage::FromJson(const std::string& json) {
    settings_.homepage_url = orbfox::utils::GetJsonString(json, "homepage_url", settings_.homepage_url);
    settings_.new_tab_url = orbfox::utils::GetJsonString(json, "new_tab_url", settings_.new_tab_url);
    settings_.restore_session = orbfox::utils::GetJsonBool(json, "restore_session", settings_.restore_session);
    settings_.tracking_protection = orbfox::utils::GetJsonBool(json, "tracking_protection", settings_.tracking_protection);

    // Validate download path before accepting
    std::string download_path = orbfox::utils::GetJsonString(json, "download_path", settings_.download_path);
    if (download_path.empty() || orbfox::utils::IsValidDownloadPath(download_path)) {
        settings_.download_path = download_path;
    }
    // else: keep existing value (reject invalid path from file)

    settings_.ask_before_download = orbfox::utils::GetJsonBool(json, "ask_before_download", settings_.ask_before_download);

    // Gesture settings
    settings_.gestures_enabled = orbfox::utils::GetJsonBool(json, "gestures_enabled", settings_.gestures_enabled);
    settings_.gesture_back_enabled = orbfox::utils::GetJsonBool(json, "gesture_back_enabled", settings_.gesture_back_enabled);
    settings_.gesture_forward_enabled = orbfox::utils::GetJsonBool(json, "gesture_forward_enabled", settings_.gesture_forward_enabled);
    settings_.gesture_close_tab_enabled = orbfox::utils::GetJsonBool(json, "gesture_close_tab_enabled", settings_.gesture_close_tab_enabled);
    settings_.gesture_reopen_tab_enabled = orbfox::utils::GetJsonBool(json, "gesture_reopen_tab_enabled", settings_.gesture_reopen_tab_enabled);

    // Parse search shortcuts (keep defaults if key is absent)
    std::string arr = orbfox::utils::GetJsonArrayContent(json, "search_shortcuts");
    if (!arr.empty()) {
        std::vector<SearchShortcut> shortcuts;
        size_t pos = 0;
        std::string obj;
        while (!(obj = orbfox::utils::GetNextJsonObject(arr, pos)).empty()) {
            SearchShortcut s;
            s.key = orbfox::utils::GetJsonString(obj, "key");
            s.url_template = orbfox::utils::GetJsonString(obj, "url_template");
            if (!s.key.empty() && !s.url_template.empty()) {
                shortcuts.push_back(std::move(s));
            }
        }
        if (!shortcuts.empty()) {
            settings_.search_shortcuts = std::move(shortcuts);
        }
    }

    return true;
}
