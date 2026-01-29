#include "window_settings.h"

#include <fstream>
#include <sstream>

#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"

std::string WindowSettings::GetSettingsPath() {
    return orbfox::utils::GetAppSupportPath() + "/window.json";
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

    settings.x = orbfox::utils::GetJsonInt(json, "x", settings.x);
    settings.y = orbfox::utils::GetJsonInt(json, "y", settings.y);
    settings.width = orbfox::utils::GetJsonInt(json, "width", settings.width);
    settings.height = orbfox::utils::GetJsonInt(json, "height", settings.height);
    settings.maximized = orbfox::utils::GetJsonBool(json, "maximized", settings.maximized);

    // Sanity checks
    if (settings.width < 400) settings.width = 400;
    if (settings.height < 300) settings.height = 300;
    if (settings.width > 4096) settings.width = 4096;
    if (settings.height > 4096) settings.height = 4096;

    return settings;
}

void WindowSettings::Save() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"x\": " << x << ",\n";
    ss << "  \"y\": " << y << ",\n";
    ss << "  \"width\": " << width << ",\n";
    ss << "  \"height\": " << height << ",\n";
    ss << "  \"maximized\": " << (maximized ? "true" : "false") << "\n";
    ss << "}\n";

    // Intentionally ignore return - Save() is best-effort
    (void)orbfox::utils::AtomicWriteFile(GetSettingsPath(), ss.str());
}
