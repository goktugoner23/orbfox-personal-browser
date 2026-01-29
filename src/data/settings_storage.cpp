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
    std::ofstream file(GetSettingsPath());
    if (!file.is_open()) {
        return;
    }
    file << ToJson();
}

void SettingsStorage::Set(const Settings& settings) {
    settings_ = settings;
    Save();
}

void SettingsStorage::SetHomepage(const std::string& url) {
    settings_.homepage_url = url;
    Save();
}

void SettingsStorage::SetNewTabUrl(const std::string& url) {
    settings_.new_tab_url = url;
    Save();
}

void SettingsStorage::SetRestoreSession(bool restore) {
    settings_.restore_session = restore;
    Save();
}

void SettingsStorage::SetTrackingProtection(bool enabled) {
    settings_.tracking_protection = enabled;
    Save();
}

void SettingsStorage::SetDownloadPath(const std::string& path) {
    // Validate path to prevent path traversal attacks
    if (!path.empty() && !orbfox::utils::IsValidDownloadPath(path)) {
        return;  // Silently reject invalid paths
    }
    settings_.download_path = path;
    Save();
}

void SettingsStorage::SetAskBeforeDownload(bool ask) {
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
    ss << "  \"ask_before_download\": " << (settings_.ask_before_download ? "true" : "false") << "\n";
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
    return true;
}
