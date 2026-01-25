#include "window_settings.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Simple JSON value extraction (avoids external dependency)
int ParseInt(const std::string& json, const std::string& key, int default_value) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos += search.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }
    // Parse number
    std::string num_str;
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-')) {
        num_str += json[pos++];
    }
    if (num_str.empty()) {
        return default_value;
    }
    return std::stoi(num_str);
}

bool ParseBool(const std::string& json, const std::string& key, bool default_value) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        return default_value;
    }
    pos += search.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
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

void EnsureDirectoryExists(const std::string& path) {
#ifdef __APPLE__
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
#endif
}

}  // namespace

std::string WindowSettings::GetSettingsPath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    std::string app_support = std::string(home) + "/Library/Application Support/PersonalBrowser";
    EnsureDirectoryExists(app_support);
    return app_support + "/window.json";
#else
    return "window.json";
#endif
}

WindowSettings WindowSettings::Load() {
    WindowSettings settings;

    std::ifstream file(GetSettingsPath());
    if (!file.is_open()) {
        return settings;  // Return defaults
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    settings.x = ParseInt(json, "x", settings.x);
    settings.y = ParseInt(json, "y", settings.y);
    settings.width = ParseInt(json, "width", settings.width);
    settings.height = ParseInt(json, "height", settings.height);
    settings.maximized = ParseBool(json, "maximized", settings.maximized);

    // Sanity checks
    if (settings.width < 400) settings.width = 400;
    if (settings.height < 300) settings.height = 300;
    if (settings.width > 4096) settings.width = 4096;
    if (settings.height > 4096) settings.height = 4096;

    return settings;
}

void WindowSettings::Save() const {
    std::ofstream file(GetSettingsPath());
    if (!file.is_open()) {
        return;
    }

    file << "{\n";
    file << "  \"x\": " << x << ",\n";
    file << "  \"y\": " << y << ",\n";
    file << "  \"width\": " << width << ",\n";
    file << "  \"height\": " << height << ",\n";
    file << "  \"maximized\": " << (maximized ? "true" : "false") << "\n";
    file << "}\n";
}
