#pragma once

#include "tab.h"

#include <memory>
#include <string>
#include <vector>

// Represents a workspace containing multiple tabs
struct Workspace {
    int id = 0;
    std::string name = "Personal";
    std::string color = "#007AFF";  // Blue by default
    std::vector<std::unique_ptr<Tab>> tabs;
    int active_tab_index = -1;

    Workspace() = default;
    explicit Workspace(int workspace_id, std::string workspace_name = "Personal")
        : id(workspace_id), name(std::move(workspace_name)) {}

    Tab* GetActiveTab() {
        if (active_tab_index >= 0 && active_tab_index < static_cast<int>(tabs.size())) {
            return tabs[active_tab_index].get();
        }
        return nullptr;
    }

    const Tab* GetActiveTab() const {
        if (active_tab_index >= 0 && active_tab_index < static_cast<int>(tabs.size())) {
            return tabs[active_tab_index].get();
        }
        return nullptr;
    }
};
