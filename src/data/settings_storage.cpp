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

std::string SettingsStorage::GetSearchUrl(const std::string& query) {
    // URL encode the query
    std::string encoded;
    for (char c : query) {
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
    return "https://www.google.com/search?q=" + encoded;
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
    ss << "  \"gesture_reopen_tab_enabled\": " << (settings_.gesture_reopen_tab_enabled ? "true" : "false") << "\n";
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

    return true;
}
