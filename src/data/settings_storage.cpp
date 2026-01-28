#include "settings_storage.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Simple JSON parsing helpers (no external dependency)

std::string ParseString(const std::string& json, const std::string& key, const std::string& default_value) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos += search.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos >= json.length() || json[pos] != '"') {
        return default_value;
    }
    ++pos;  // Skip opening quote

    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            ++pos;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    return result;
}

bool ParseBool(const std::string& json, const std::string& key, bool default_value) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos += search.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        ++pos;
    }
    if (json.substr(pos, 4) == "true") {
        return true;
    }
    if (json.substr(pos, 5) == "false") {
        return false;
    }
    return default_value;
}

std::string EscapeJsonString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

void EnsureDirectoryExists(const std::string& path) {
#ifdef __APPLE__
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
#endif
}

std::string GetAppSupportPath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    std::string app_support = std::string(home) + "/Library/Application Support/OrbFox";
    EnsureDirectoryExists(app_support);
    return app_support;
#else
    return ".";
#endif
}

std::string ExpandTilde(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    return std::string(home) + path.substr(1);
#else
    return path;
#endif
}

}  // namespace

SettingsStorage& SettingsStorage::GetInstance() {
    static SettingsStorage instance;
    return instance;
}

std::string SettingsStorage::GetSettingsPath() const {
    return GetAppSupportPath() + "/settings.json";
}

void SettingsStorage::Load() {
    std::ifstream file(GetSettingsPath());
    if (!file.is_open()) {
        return;  // Use defaults
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    FromJson(json);
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
#ifdef __APPLE__
        const char* home = std::getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        return std::string(home) + "/Downloads";
#else
        return ".";
#endif
    }
    return ExpandTilde(settings_.download_path);
}

std::string SettingsStorage::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"homepage_url\": \"" << EscapeJsonString(settings_.homepage_url) << "\",\n";
    ss << "  \"new_tab_url\": \"" << EscapeJsonString(settings_.new_tab_url) << "\",\n";
    ss << "  \"restore_session\": " << (settings_.restore_session ? "true" : "false") << ",\n";
    ss << "  \"tracking_protection\": " << (settings_.tracking_protection ? "true" : "false") << ",\n";
    ss << "  \"download_path\": \"" << EscapeJsonString(settings_.download_path) << "\",\n";
    ss << "  \"ask_before_download\": " << (settings_.ask_before_download ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}

bool SettingsStorage::FromJson(const std::string& json) {
    settings_.homepage_url = ParseString(json, "homepage_url", settings_.homepage_url);
    settings_.new_tab_url = ParseString(json, "new_tab_url", settings_.new_tab_url);
    settings_.restore_session = ParseBool(json, "restore_session", settings_.restore_session);
    settings_.tracking_protection = ParseBool(json, "tracking_protection", settings_.tracking_protection);
    settings_.download_path = ParseString(json, "download_path", settings_.download_path);
    settings_.ask_before_download = ParseBool(json, "ask_before_download", settings_.ask_before_download);
    return true;
}
