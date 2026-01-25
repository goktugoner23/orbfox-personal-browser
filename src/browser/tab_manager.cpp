#include "tab_manager.h"

TabManager::TabManager() {
    // Create default workspace
    CreateWorkspace("Personal");
}

TabManager::~TabManager() = default;

Workspace* TabManager::CreateWorkspace(const std::string& name) {
    auto workspace = std::make_unique<Workspace>(next_workspace_id_++, name);
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
        int index = static_cast<int>(std::distance(workspaces_.begin(), it));
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

void TabManager::CloseTab(int tab_id) {
    Workspace* workspace = GetActiveWorkspace();
    if (!workspace) {
        return;
    }

    auto it = std::find_if(workspace->tabs.begin(), workspace->tabs.end(),
        [tab_id](const auto& t) { return t->id == tab_id; });

    if (it != workspace->tabs.end()) {
        Tab* tab = it->get();

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
