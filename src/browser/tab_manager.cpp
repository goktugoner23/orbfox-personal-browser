#include "tab_manager.h"

#include <chrono>
#include <regex>

namespace {

// Extract domain from URL for comparison
// Returns empty string if URL is invalid or has no host
std::string ExtractDomain(const std::string& url) {
    // Simple regex to extract host from URL
    // Matches: scheme://[user:pass@]host[:port][/path]
    static const std::regex url_regex(R"(^[a-zA-Z][a-zA-Z0-9+.-]*://(?:[^@/]*@)?([^:/?#]+))");
    std::smatch match;
    if (std::regex_search(url, match, url_regex) && match.size() > 1) {
        std::string host = match[1].str();
        // Remove www. prefix for comparison
        if (host.substr(0, 4) == "www.") {
            host = host.substr(4);
        }
        return host;
    }
    return "";
}

}  // namespace

TabManager::TabManager() {
    // Create default workspace
    CreateWorkspace("WS 1");
}

TabManager::~TabManager() = default;

bool TabManager::WorkspaceNameExists(const std::string& name) const {
    for (const auto& ws : workspaces_) {
        if (ws->name == name) {
            return true;
        }
    }
    return false;
}

std::string TabManager::GenerateUniqueWorkspaceName() const {
    int num = 1;
    std::string name;
    do {
        name = "WS " + std::to_string(num);
        num++;
    } while (WorkspaceNameExists(name));
    return name;
}

Workspace* TabManager::CreateWorkspace(const std::string& name) {
    // If name is empty or "New Workspace", generate a unique name
    std::string final_name = name;
    if (final_name.empty() || final_name == "New Workspace") {
        final_name = GenerateUniqueWorkspaceName();
    } else if (WorkspaceNameExists(final_name)) {
        // If name already exists, append a number
        int suffix = 2;
        std::string base_name = final_name;
        while (WorkspaceNameExists(final_name)) {
            final_name = base_name + " " + std::to_string(suffix);
            suffix++;
        }
    }

    auto workspace = std::make_unique<Workspace>(next_workspace_id_++, final_name);
    // Assign color from palette based on workspace count
    workspace->color = WorkspaceColors::ForIndex(static_cast<int>(workspaces_.size()));
    Workspace* ptr = workspace.get();
    workspaces_.push_back(std::move(workspace));

    if (callbacks_.on_workspace_changed) {
        callbacks_.on_workspace_changed(ptr);
    }

    return ptr;
}

void TabManager::DeleteWorkspace(int workspace_id) {
    // Don't delete if it's the only workspace
    if (workspaces_.size() <= 1) {
        return;
    }

    auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
        [workspace_id](const auto& w) { return w->id == workspace_id; });

    if (it != workspaces_.end()) {
        workspaces_.erase(it);

        // Adjust active workspace index if needed
        if (active_workspace_index_ >= static_cast<int>(workspaces_.size())) {
            active_workspace_index_ = static_cast<int>(workspaces_.size()) - 1;
        }

        if (callbacks_.on_workspace_changed) {
            callbacks_.on_workspace_changed(GetActiveWorkspace());
        }
    }
}

void TabManager::SetActiveWorkspace(int workspace_id) {
    for (size_t i = 0; i < workspaces_.size(); ++i) {
        if (workspaces_[i]->id == workspace_id) {
            active_workspace_index_ = static_cast<int>(i);
            if (callbacks_.on_workspace_changed) {
                callbacks_.on_workspace_changed(workspaces_[i].get());
            }
            break;
        }
    }
}

Workspace* TabManager::GetActiveWorkspace() {
    if (active_workspace_index_ >= 0 &&
        active_workspace_index_ < static_cast<int>(workspaces_.size())) {
        return workspaces_[active_workspace_index_].get();
    }
    return nullptr;
}

Tab* TabManager::CreateTab(const std::string& url) {
    Workspace* workspace = GetActiveWorkspace();
    if (!workspace) {
        return nullptr;
    }

    auto tab = std::make_unique<Tab>(next_tab_id_++);
    tab->url = url;
    Tab* ptr = tab.get();

    workspace->tabs.push_back(std::move(tab));
    workspace->active_tab_index = static_cast<int>(workspace->tabs.size()) - 1;

    if (callbacks_.on_tab_created) {
        callbacks_.on_tab_created(ptr);
    }
    if (callbacks_.on_tab_activated) {
        callbacks_.on_tab_activated(ptr);
    }

    return ptr;
}

Tab* TabManager::CreateTabInBackground(const std::string& url) {
    Workspace* workspace = GetActiveWorkspace();
    if (!workspace) {
        return nullptr;
    }

    auto tab = std::make_unique<Tab>(next_tab_id_++);
    tab->url = url;
    Tab* ptr = tab.get();

    workspace->tabs.push_back(std::move(tab));
    // Don't change active_tab_index - keep current tab active

    if (callbacks_.on_tab_created) {
        callbacks_.on_tab_created(ptr);
    }
    // Don't call on_tab_activated - the new tab stays in background

    return ptr;
}

void TabManager::CloseTab(int tab_id) {
    Workspace* workspace = GetActiveWorkspace();
    if (!workspace) {
        return;
    }

    auto it = std::find_if(workspace->tabs.begin(), workspace->tabs.end(),
        [tab_id](const auto& t) { return t->id == tab_id; });

    if (it != workspace->tabs.end()) {
        Tab* tab = it->get();

        // Save tab info for reopening (only if it has a URL)
        if (!tab->url.empty()) {
            ClosedTab closed;
            closed.url = tab->url;
            closed.title = tab->title;
            closed.workspace_id = workspace->id;
            recently_closed_tabs_.push_front(closed);

            // Limit the number of closed tabs we track
            if (recently_closed_tabs_.size() > kMaxClosedTabs) {
                recently_closed_tabs_.pop_back();
            }
        }

        if (callbacks_.on_tab_closed) {
            callbacks_.on_tab_closed(tab);
        }

        int index = static_cast<int>(std::distance(workspace->tabs.begin(), it));
        workspace->tabs.erase(it);

        // Adjust active tab index
        if (workspace->tabs.empty()) {
            workspace->active_tab_index = -1;
        } else if (workspace->active_tab_index >= static_cast<int>(workspace->tabs.size())) {
            workspace->active_tab_index = static_cast<int>(workspace->tabs.size()) - 1;
        } else if (index <= workspace->active_tab_index && workspace->active_tab_index > 0) {
            workspace->active_tab_index--;
        }

        // Notify new active tab
        if (workspace->active_tab_index >= 0 && callbacks_.on_tab_activated) {
            callbacks_.on_tab_activated(workspace->tabs[workspace->active_tab_index].get());
        }
    }
}

void TabManager::SetActiveTab(int tab_id) {
    Workspace* workspace = GetActiveWorkspace();
    if (!workspace) {
        return;
    }

    for (size_t i = 0; i < workspace->tabs.size(); ++i) {
        if (workspace->tabs[i]->id == tab_id) {
            workspace->active_tab_index = static_cast<int>(i);
            if (callbacks_.on_tab_activated) {
                callbacks_.on_tab_activated(workspace->tabs[i].get());
            }
            break;
        }
    }
}

Tab* TabManager::GetActiveTab() {
    Workspace* workspace = GetActiveWorkspace();
    if (workspace) {
        return workspace->GetActiveTab();
    }
    return nullptr;
}

Tab* TabManager::GetTabById(int tab_id) {
    for (const auto& workspace : workspaces_) {
        for (const auto& tab : workspace->tabs) {
            if (tab->id == tab_id) {
                return tab.get();
            }
        }
    }
    return nullptr;
}

#ifndef UNIT_TEST
Tab* TabManager::GetTabByBrowser(CefRefPtr<CefBrowser> browser) {
    if (!browser) {
        return nullptr;
    }

    for (const auto& workspace : workspaces_) {
        for (const auto& tab : workspace->tabs) {
            if (tab->browser && tab->browser->IsSame(browser)) {
                return tab.get();
            }
        }
    }
    return nullptr;
}
#endif

void TabManager::UpdateTabTitle(int tab_id, const std::string& title) {
    Tab* tab = GetTabById(tab_id);
    if (tab) {
        tab->title = title;
        if (callbacks_.on_tab_updated) {
            callbacks_.on_tab_updated(tab);
        }
    }
}

void TabManager::UpdateTabUrl(int tab_id, const std::string& url) {
    Tab* tab = GetTabById(tab_id);
    if (tab) {
        // Only clear favicon data when navigating to a different domain
        // This prevents losing favicons due to minor URL changes (trailing slash,
        // fragments, query params) while still clearing when navigating to a new site
        if (tab->url != url) {
            std::string old_domain = ExtractDomain(tab->url);
            std::string new_domain = ExtractDomain(url);

            // Clear favicon only if domain actually changed (navigating to different site)
            if (!old_domain.empty() && !new_domain.empty() && old_domain != new_domain) {
                tab->favicon_url.clear();
                tab->favicon_data.clear();
            }
        }

        tab->url = url;

        if (callbacks_.on_tab_updated) {
            callbacks_.on_tab_updated(tab);
        }
    }
}

void TabManager::UpdateTabLoadingState(int tab_id, bool is_loading) {
    Tab* tab = GetTabById(tab_id);
    if (tab) {
        tab->is_loading = is_loading;
        if (callbacks_.on_tab_updated) {
            callbacks_.on_tab_updated(tab);
        }
    }
}

void TabManager::UpdateTabFavicon(int tab_id, const std::string& favicon_url,
                                   const std::vector<unsigned char>& png_data) {
    Tab* tab = GetTabById(tab_id);
    if (tab) {
        tab->favicon_url = favicon_url;
        tab->favicon_data = png_data;
        if (callbacks_.on_tab_updated) {
            callbacks_.on_tab_updated(tab);
        }
    }
}

bool TabManager::ReopenClosedTab() {
    if (recently_closed_tabs_.empty()) {
        return false;
    }

    ClosedTab closed = recently_closed_tabs_.front();
    recently_closed_tabs_.pop_front();

    // Try to find the original workspace
    Workspace* target_workspace = nullptr;
    for (const auto& ws : workspaces_) {
        if (ws->id == closed.workspace_id) {
            target_workspace = ws.get();
            break;
        }
    }

    // Fall back to active workspace if original doesn't exist
    if (!target_workspace) {
        target_workspace = GetActiveWorkspace();
    }

    if (!target_workspace) {
        return false;
    }

    // Switch to the target workspace if different
    if (target_workspace != GetActiveWorkspace()) {
        SetActiveWorkspace(target_workspace->id);
    }

    // Create the tab with the saved URL
    Tab* tab = CreateTab(closed.url);
    if (tab) {
        tab->title = closed.title;
        return true;
    }

    return false;
}

bool TabManager::MoveTabToWorkspace(int tab_id, int target_workspace_id) {
    // Find the source workspace and tab
    Workspace* source_workspace = nullptr;
    std::unique_ptr<Tab> tab_to_move;
    int source_tab_index = -1;

    for (auto& workspace : workspaces_) {
        for (size_t i = 0; i < workspace->tabs.size(); ++i) {
            if (workspace->tabs[i]->id == tab_id) {
                source_workspace = workspace.get();
                source_tab_index = static_cast<int>(i);
                break;
            }
        }
        if (source_workspace) break;
    }

    if (!source_workspace || source_tab_index < 0) {
        return false;  // Tab not found
    }

    // Find target workspace
    Workspace* target_workspace = nullptr;
    for (auto& workspace : workspaces_) {
        if (workspace->id == target_workspace_id) {
            target_workspace = workspace.get();
            break;
        }
    }

    if (!target_workspace) {
        return false;  // Target workspace not found
    }

    // Don't move to same workspace
    if (source_workspace == target_workspace) {
        return false;
    }

    // Extract the tab from source
    tab_to_move = std::move(source_workspace->tabs[source_tab_index]);
    source_workspace->tabs.erase(source_workspace->tabs.begin() + source_tab_index);

    // Adjust source workspace's active tab index
    if (source_workspace->tabs.empty()) {
        source_workspace->active_tab_index = -1;
    } else if (source_workspace->active_tab_index >= static_cast<int>(source_workspace->tabs.size())) {
        source_workspace->active_tab_index = static_cast<int>(source_workspace->tabs.size()) - 1;
    } else if (source_tab_index <= source_workspace->active_tab_index && source_workspace->active_tab_index > 0) {
        source_workspace->active_tab_index--;
    }

    // Add to target workspace
    Tab* moved_tab = tab_to_move.get();
    target_workspace->tabs.push_back(std::move(tab_to_move));
    target_workspace->active_tab_index = static_cast<int>(target_workspace->tabs.size()) - 1;

    // Notify callbacks
    if (callbacks_.on_workspace_changed) {
        callbacks_.on_workspace_changed(source_workspace);
    }

    // Switch to target workspace and activate the moved tab
    SetActiveWorkspace(target_workspace_id);

    if (callbacks_.on_tab_activated) {
        callbacks_.on_tab_activated(moved_tab);
    }

    return true;
}

bool TabManager::ReorderTab(int tab_id, int new_index) {
    // Find the workspace containing the tab
    Workspace* workspace = nullptr;
    int current_index = -1;

    for (auto& ws : workspaces_) {
        for (size_t i = 0; i < ws->tabs.size(); ++i) {
            if (ws->tabs[i]->id == tab_id) {
                workspace = ws.get();
                current_index = static_cast<int>(i);
                break;
            }
        }
        if (workspace) break;
    }

    if (!workspace || current_index < 0) {
        return false;  // Tab not found
    }

    // Validate new index
    int max_index = static_cast<int>(workspace->tabs.size()) - 1;
    if (new_index < 0) new_index = 0;
    if (new_index > max_index) new_index = max_index;

    if (current_index == new_index) {
        return true;  // Already in position
    }

    // Move the tab
    auto tab = std::move(workspace->tabs[current_index]);
    workspace->tabs.erase(workspace->tabs.begin() + current_index);
    workspace->tabs.insert(workspace->tabs.begin() + new_index, std::move(tab));

    // Update active tab index if needed
    if (workspace->active_tab_index == current_index) {
        workspace->active_tab_index = new_index;
    } else if (current_index < workspace->active_tab_index && new_index >= workspace->active_tab_index) {
        workspace->active_tab_index--;
    } else if (current_index > workspace->active_tab_index && new_index <= workspace->active_tab_index) {
        workspace->active_tab_index++;
    }

    // Notify callbacks
    if (callbacks_.on_workspace_changed) {
        callbacks_.on_workspace_changed(workspace);
    }

    return true;
}

bool TabManager::ReorderWorkspace(int workspace_id, int new_index) {
    // Find current index
    int current_index = -1;
    for (size_t i = 0; i < workspaces_.size(); ++i) {
        if (workspaces_[i]->id == workspace_id) {
            current_index = static_cast<int>(i);
            break;
        }
    }

    if (current_index < 0) {
        return false;  // Workspace not found
    }

    // Validate new index
    int max_index = static_cast<int>(workspaces_.size()) - 1;
    if (new_index < 0) new_index = 0;
    if (new_index > max_index) new_index = max_index;

    if (current_index == new_index) {
        return true;  // Already in position
    }

    // Move the workspace
    auto workspace = std::move(workspaces_[current_index]);
    workspaces_.erase(workspaces_.begin() + current_index);
    workspaces_.insert(workspaces_.begin() + new_index, std::move(workspace));

    // Update active workspace index if needed
    if (active_workspace_index_ == current_index) {
        active_workspace_index_ = new_index;
    } else if (current_index < active_workspace_index_ && new_index >= active_workspace_index_) {
        active_workspace_index_--;
    } else if (current_index > active_workspace_index_ && new_index <= active_workspace_index_) {
        active_workspace_index_++;
    }

    return true;
}

// Tab hibernation methods

static int64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

bool TabManager::HibernateTab(int tab_id) {
    Tab* tab = GetTabById(tab_id);
    if (!tab) return false;

    // Don't hibernate pinned tabs or already hibernated tabs
    if (tab->is_pinned || tab->is_hibernated) return false;

    // Don't hibernate the active tab
    Tab* activeTab = GetActiveTab();
    if (activeTab && activeTab->id == tab_id) return false;

    tab->is_hibernated = true;

    if (callbacks_.on_tab_hibernated) {
        callbacks_.on_tab_hibernated(tab);
    }

    return true;
}

bool TabManager::WakeTab(int tab_id) {
    Tab* tab = GetTabById(tab_id);
    if (!tab || !tab->is_hibernated) return false;

    tab->is_hibernated = false;
    tab->last_active_time = GetCurrentTimestamp();

    if (callbacks_.on_tab_woken) {
        callbacks_.on_tab_woken(tab);
    }

    return true;
}

void TabManager::HibernateInactiveTabs(int64_t inactive_seconds) {
    int64_t now = GetCurrentTimestamp();
    int64_t threshold = now - inactive_seconds;

    for (auto& workspace : workspaces_) {
        for (auto& tab : workspace->tabs) {
            // Skip pinned, hibernated, loading, or recently active tabs
            if (tab->is_pinned || tab->is_hibernated || tab->is_loading) continue;

            // Skip the active tab in the active workspace
            if (workspace.get() == GetActiveWorkspace() &&
                tab.get() == workspace->GetActiveTab()) continue;

            // Hibernate if inactive for too long
            if (tab->last_active_time > 0 && tab->last_active_time < threshold) {
                HibernateTab(tab->id);
            }
        }
    }
}

void TabManager::UpdateTabActiveTime(int tab_id) {
    Tab* tab = GetTabById(tab_id);
    if (tab) {
        tab->last_active_time = GetCurrentTimestamp();
    }
}
