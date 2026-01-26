#pragma once

#include "workspace.h"

#include <functional>
#include <memory>
#include <vector>

#ifndef UNIT_TEST
#include "include/cef_browser.h"
#endif

// Callback types for UI updates
struct TabManagerCallbacks {
    std::function<void(Tab*)> on_tab_created;
    std::function<void(Tab*)> on_tab_closed;
    std::function<void(Tab*)> on_tab_activated;
    std::function<void(Tab*)> on_tab_updated;
    std::function<void(Workspace*)> on_workspace_changed;
};

// Manages tabs and workspaces
class TabManager {
public:
    TabManager();
    ~TabManager();

    // Workspace management
    Workspace* CreateWorkspace(const std::string& name = "New Workspace");
    void DeleteWorkspace(int workspace_id);
    void SetActiveWorkspace(int workspace_id);
    Workspace* GetActiveWorkspace();
    const std::vector<std::unique_ptr<Workspace>>& GetWorkspaces() const { return workspaces_; }

    // Tab management
    Tab* CreateTab(const std::string& url = "");
    void CloseTab(int tab_id);
    void SetActiveTab(int tab_id);
    Tab* GetActiveTab();
    Tab* GetTabById(int tab_id);
#ifndef UNIT_TEST
    Tab* GetTabByBrowser(CefRefPtr<CefBrowser> browser);
#endif

    // Tab operations
    void UpdateTabTitle(int tab_id, const std::string& title);
    void UpdateTabUrl(int tab_id, const std::string& url);
    void UpdateTabLoadingState(int tab_id, bool is_loading);
    void UpdateTabFavicon(int tab_id, const std::string& favicon_url, const std::vector<unsigned char>& png_data);

    // Set callbacks
    void SetCallbacks(const TabManagerCallbacks& callbacks) { callbacks_ = callbacks; }

    // Get next tab ID
    int GetNextTabId() { return next_tab_id_++; }

private:
    std::vector<std::unique_ptr<Workspace>> workspaces_;
    int active_workspace_index_ = 0;
    int next_tab_id_ = 1;
    int next_workspace_id_ = 1;
    TabManagerCallbacks callbacks_;
};
