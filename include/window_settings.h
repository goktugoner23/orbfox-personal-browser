#pragma once

#include <string>

// Window position and size settings with persistence
struct WindowSettings {
    int x = 100;
    int y = 100;
    int width = 1280;
    int height = 800;
    bool maximized = false;

    // Load settings from disk (returns defaults if file doesn't exist)
    static WindowSettings Load();

    // Save settings to disk
    void Save() const;

    // Get the settings file path
    static std::string GetSettingsPath();
};
