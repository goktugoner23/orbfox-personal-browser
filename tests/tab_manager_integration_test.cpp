#include "tab_manager.h"
#include "gtest/gtest.h"

#include <vector>
#include <string>

// ============================================================================
// Tab Manager Integration Tests
// Tests that verify multi-workspace operations and callback sequences
// ============================================================================

class TabManagerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<TabManager>();
        ResetCallbackTracking();
    }

    void TearDown() override {
        manager_.reset();
    }

    void ResetCallbackTracking() {
        created_tabs_.clear();
        closed_tabs_.clear();
        activated_tabs_.clear();
        updated_tabs_.clear();
        changed_workspaces_.clear();
    }

    void SetupCallbacks() {
        TabManagerCallbacks callbacks;
        callbacks.on_tab_created = [this](Tab* tab) {
            if (tab) created_tabs_.push_back(tab->id);
        };
        callbacks.on_tab_closed = [this](Tab* tab) {
            if (tab) closed_tabs_.push_back(tab->id);
        };
        callbacks.on_tab_activated = [this](Tab* tab) {
            if (tab) activated_tabs_.push_back(tab->id);
        };
        callbacks.on_tab_updated = [this](Tab* tab) {
            if (tab) updated_tabs_.push_back(tab->id);
        };
        callbacks.on_workspace_changed = [this](Workspace* ws) {
            if (ws) changed_workspaces_.push_back(ws->id);
        };
        manager_->SetCallbacks(callbacks);
    }

    std::unique_ptr<TabManager> manager_;
    std::vector<int> created_tabs_;
    std::vector<int> closed_tabs_;
    std::vector<int> activated_tabs_;
    std::vector<int> updated_tabs_;
    std::vector<int> changed_workspaces_;
};

// ============================================================================
// Multi-Workspace Tab Operations
// ============================================================================

TEST_F(TabManagerIntegrationTest, CreateTabsAcrossWorkspaces) {
    // Default workspace already exists (WS 1)
    Tab* tab1 = manager_->CreateTab("https://ws1-tab1.com");
    Tab* tab2 = manager_->CreateTab("https://ws1-tab2.com");

    // Create second workspace
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);

    Tab* tab3 = manager_->CreateTab("https://ws2-tab1.com");

    // Verify workspace 1 has 2 tabs
    Workspace* ws1 = manager_->GetWorkspaces()[0].get();
    EXPECT_EQ(ws1->tabs.size(), 2u);
    EXPECT_EQ(ws1->tabs[0]->url, "https://ws1-tab1.com");
    EXPECT_EQ(ws1->tabs[1]->url, "https://ws1-tab2.com");

    // Verify workspace 2 has 1 tab
    EXPECT_EQ(ws2->tabs.size(), 1u);
    EXPECT_EQ(ws2->tabs[0]->url, "https://ws2-tab1.com");

    // Tab IDs should be unique across workspaces
    EXPECT_NE(tab1->id, tab2->id);
    EXPECT_NE(tab2->id, tab3->id);
    EXPECT_NE(tab1->id, tab3->id);
}

TEST_F(TabManagerIntegrationTest, SwitchBetweenWorkspaces) {
    // Create tabs in WS 1
    Tab* tab1 = manager_->CreateTab("https://site1.com");

    // Create WS 2 and switch to it
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);
    Tab* tab2 = manager_->CreateTab("https://site2.com");

    // Active tab should be from WS 2
    Tab* activeTab = manager_->GetActiveTab();
    EXPECT_EQ(activeTab->id, tab2->id);

    // Switch back to WS 1
    Workspace* ws1 = manager_->GetWorkspaces()[0].get();
    manager_->SetActiveWorkspace(ws1->id);

    // Active tab should be from WS 1
    activeTab = manager_->GetActiveTab();
    EXPECT_EQ(activeTab->id, tab1->id);
}

TEST_F(TabManagerIntegrationTest, DeleteWorkspaceWithTabs) {
    // Create tabs in WS 1
    manager_->CreateTab("https://site1.com");
    manager_->CreateTab("https://site2.com");

    // Create WS 2
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);
    manager_->CreateTab("https://site3.com");

    EXPECT_EQ(manager_->GetWorkspaces().size(), 2u);

    // Delete WS 2 (should switch to WS 1)
    int ws2Id = ws2->id;
    manager_->DeleteWorkspace(ws2Id);

    EXPECT_EQ(manager_->GetWorkspaces().size(), 1u);

    // Active workspace should be WS 1
    Workspace* activeWs = manager_->GetActiveWorkspace();
    EXPECT_EQ(activeWs->name, "WS 1");
    EXPECT_EQ(activeWs->tabs.size(), 2u);
}

TEST_F(TabManagerIntegrationTest, DuplicateWorkspaceTabsPreservation) {
    // Create tabs in WS 1
    Tab* tab1 = manager_->CreateTab("https://site1.com");
    tab1->title = "Site 1";
    Tab* tab2 = manager_->CreateTab("https://site2.com");
    tab2->title = "Site 2";

    // Get WS 1 state
    Workspace* ws1 = manager_->GetActiveWorkspace();
    ASSERT_EQ(ws1->tabs.size(), 2u);

    // Note: TabManager doesn't have a DuplicateWorkspace method built-in
    // This test verifies the state we would need to preserve
    std::vector<std::string> originalUrls;
    for (const auto& tab : ws1->tabs) {
        originalUrls.push_back(tab->url);
    }

    EXPECT_EQ(originalUrls.size(), 2u);
    EXPECT_EQ(originalUrls[0], "https://site1.com");
    EXPECT_EQ(originalUrls[1], "https://site2.com");
}

// ============================================================================
// Workspace Lifecycle
// ============================================================================

TEST_F(TabManagerIntegrationTest, WorkspaceCreationIncrementsId) {
    Workspace* ws1 = manager_->GetActiveWorkspace();
    int ws1Id = ws1->id;

    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    EXPECT_GT(ws2->id, ws1Id);

    Workspace* ws3 = manager_->CreateWorkspace("WS 3");
    EXPECT_GT(ws3->id, ws2->id);
}

TEST_F(TabManagerIntegrationTest, DeleteWorkspaceAdjustsActiveIndex) {
    // Create 3 workspaces
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    Workspace* ws3 = manager_->CreateWorkspace("WS 3");

    // Switch to WS 3 (index 2)
    manager_->SetActiveWorkspace(ws3->id);
    EXPECT_EQ(manager_->GetActiveWorkspace()->name, "WS 3");

    // Delete WS 2 (index 1)
    manager_->DeleteWorkspace(ws2->id);

    // Should still be on WS 3, but now at index 1
    EXPECT_EQ(manager_->GetActiveWorkspace()->name, "WS 3");
    EXPECT_EQ(manager_->GetWorkspaces().size(), 2u);
}

TEST_F(TabManagerIntegrationTest, LastWorkspaceDeletionPrevented) {
    // Default has 1 workspace
    EXPECT_EQ(manager_->GetWorkspaces().size(), 1u);

    Workspace* ws1 = manager_->GetActiveWorkspace();

    // Try to delete the only workspace
    manager_->DeleteWorkspace(ws1->id);

    // Should still have 1 workspace
    EXPECT_EQ(manager_->GetWorkspaces().size(), 1u);
}

TEST_F(TabManagerIntegrationTest, CreateSwitchDeleteSequence) {
    // Create several workspaces
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    Workspace* ws3 = manager_->CreateWorkspace("WS 3");

    EXPECT_EQ(manager_->GetWorkspaces().size(), 3u);

    // Add tabs to each
    manager_->SetActiveWorkspace(manager_->GetWorkspaces()[0]->id);
    manager_->CreateTab("https://ws1.com");

    manager_->SetActiveWorkspace(ws2->id);
    manager_->CreateTab("https://ws2.com");

    manager_->SetActiveWorkspace(ws3->id);
    manager_->CreateTab("https://ws3.com");

    // Delete middle workspace
    manager_->DeleteWorkspace(ws2->id);

    EXPECT_EQ(manager_->GetWorkspaces().size(), 2u);
    EXPECT_EQ(manager_->GetWorkspaces()[0]->name, "WS 1");
    EXPECT_EQ(manager_->GetWorkspaces()[1]->name, "WS 3");
}

// ============================================================================
// Callback Integration
// ============================================================================

TEST_F(TabManagerIntegrationTest, TabCreatedCallbackFires) {
    SetupCallbacks();

    Tab* tab = manager_->CreateTab("https://test.google.com");

    ASSERT_EQ(created_tabs_.size(), 1u);
    EXPECT_EQ(created_tabs_[0], tab->id);
}

TEST_F(TabManagerIntegrationTest, TabActivatedAfterCreation) {
    SetupCallbacks();

    Tab* tab = manager_->CreateTab("https://test.google.com");

    // Both created and activated should fire
    ASSERT_EQ(created_tabs_.size(), 1u);
    ASSERT_EQ(activated_tabs_.size(), 1u);
    EXPECT_EQ(activated_tabs_[0], tab->id);
}

TEST_F(TabManagerIntegrationTest, TabClosedCallbackFires) {
    SetupCallbacks();

    Tab* tab = manager_->CreateTab("https://test.google.com");
    int tabId = tab->id;

    manager_->CloseTab(tabId);

    ASSERT_EQ(closed_tabs_.size(), 1u);
    EXPECT_EQ(closed_tabs_[0], tabId);
}

TEST_F(TabManagerIntegrationTest, WorkspaceChangedCallbackFires) {
    SetupCallbacks();

    // First workspace change happens at construction
    ResetCallbackTracking();

    Workspace* ws2 = manager_->CreateWorkspace("WS 2");

    // Workspace creation should trigger workspace_changed
    ASSERT_GE(changed_workspaces_.size(), 1u);
    EXPECT_EQ(changed_workspaces_.back(), ws2->id);
}

TEST_F(TabManagerIntegrationTest, MultipleOperationsCallbackSequence) {
    SetupCallbacks();

    // Create tab 1
    Tab* tab1 = manager_->CreateTab("https://site1.com");
    int tab1Id = tab1->id;  // Save ID before close invalidates pointer

    // Create tab 2
    Tab* tab2 = manager_->CreateTab("https://site2.com");
    int tab2Id = tab2->id;

    // Close tab 1 (invalidates tab1 pointer)
    manager_->CloseTab(tab1Id);

    // Verify callback sequence
    EXPECT_EQ(created_tabs_.size(), 2u);
    EXPECT_EQ(created_tabs_[0], tab1Id);
    EXPECT_EQ(created_tabs_[1], tab2Id);

    EXPECT_EQ(closed_tabs_.size(), 1u);
    EXPECT_EQ(closed_tabs_[0], tab1Id);

    // Tab 2 should become active after tab 1 is closed
    // (activated_tabs may have multiple entries)
    EXPECT_GT(activated_tabs_.size(), 0u);
}

TEST_F(TabManagerIntegrationTest, TabUpdateCallbackFires) {
    SetupCallbacks();

    Tab* tab = manager_->CreateTab("https://test.google.com");
    ResetCallbackTracking();

    manager_->UpdateTabTitle(tab->id, "New Title");

    ASSERT_EQ(updated_tabs_.size(), 1u);
    EXPECT_EQ(updated_tabs_[0], tab->id);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TabManagerIntegrationTest, CreateTabInEmptyWorkspace) {
    // Default workspace is empty
    Workspace* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 0u);
    EXPECT_EQ(ws->active_tab_index, -1);

    // Create first tab
    Tab* tab = manager_->CreateTab("https://test.google.com");

    EXPECT_EQ(ws->tabs.size(), 1u);
    EXPECT_EQ(ws->active_tab_index, 0);
    EXPECT_EQ(ws->GetActiveTab(), tab);
}

TEST_F(TabManagerIntegrationTest, DeleteAllTabsInWorkspace) {
    Tab* tab1 = manager_->CreateTab("https://site1.com");
    Tab* tab2 = manager_->CreateTab("https://site2.com");

    Workspace* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 2u);

    manager_->CloseTab(tab1->id);
    EXPECT_EQ(ws->tabs.size(), 1u);
    EXPECT_EQ(ws->GetActiveTab(), tab2);

    manager_->CloseTab(tab2->id);
    EXPECT_EQ(ws->tabs.size(), 0u);
    EXPECT_EQ(ws->active_tab_index, -1);
    EXPECT_EQ(ws->GetActiveTab(), nullptr);
}

TEST_F(TabManagerIntegrationTest, CloseMiddleTabAdjustsActiveIndex) {
    manager_->CreateTab("https://site1.com");  // tab1
    Tab* tab2 = manager_->CreateTab("https://site2.com");
    manager_->CreateTab("https://site3.com");  // tab3

    // Make tab2 active
    manager_->SetActiveTab(tab2->id);

    Workspace* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->active_tab_index, 1);

    // Close tab2 (the active one)
    manager_->CloseTab(tab2->id);

    // Active should move to next available
    EXPECT_EQ(ws->tabs.size(), 2u);
    EXPECT_GE(ws->active_tab_index, 0);
    EXPECT_LT(ws->active_tab_index, 2);
}

TEST_F(TabManagerIntegrationTest, GetTabByIdAcrossWorkspaces) {
    // Create tab in WS 1
    Tab* tab1 = manager_->CreateTab("https://site1.com");

    // Create WS 2 and add tab
    Workspace* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);
    Tab* tab2 = manager_->CreateTab("https://site2.com");

    // Should find tabs regardless of active workspace
    EXPECT_EQ(manager_->GetTabById(tab1->id), tab1);
    EXPECT_EQ(manager_->GetTabById(tab2->id), tab2);

    // Switch to WS 1
    manager_->SetActiveWorkspace(manager_->GetWorkspaces()[0]->id);

    // Should still find both
    EXPECT_EQ(manager_->GetTabById(tab1->id), tab1);
    EXPECT_EQ(manager_->GetTabById(tab2->id), tab2);
}

TEST_F(TabManagerIntegrationTest, NonExistentTabReturnsNull) {
    Tab* tab = manager_->GetTabById(99999);
    EXPECT_EQ(tab, nullptr);
}

TEST_F(TabManagerIntegrationTest, CloseNonExistentTabNoOp) {
    SetupCallbacks();

    manager_->CloseTab(99999);

    // No callbacks should fire
    EXPECT_EQ(closed_tabs_.size(), 0u);
}

TEST_F(TabManagerIntegrationTest, SetActiveTabNonExistentNoOp) {
    SetupCallbacks();
    Tab* tab1 = manager_->CreateTab("https://site1.com");
    ResetCallbackTracking();

    manager_->SetActiveTab(99999);

    // No activation callback for invalid tab
    EXPECT_EQ(activated_tabs_.size(), 0u);

    // Original tab still active
    EXPECT_EQ(manager_->GetActiveTab(), tab1);
}

TEST_F(TabManagerIntegrationTest, SetActiveWorkspaceNonExistent) {
    Workspace* original = manager_->GetActiveWorkspace();
    int originalId = original->id;

    manager_->SetActiveWorkspace(99999);

    // Should stay on original workspace
    EXPECT_EQ(manager_->GetActiveWorkspace()->id, originalId);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(TabManagerIntegrationTest, ManyTabsInOneWorkspace) {
    const int NUM_TABS = 50;

    for (int i = 0; i < NUM_TABS; ++i) {
        std::string url = "https://site" + std::to_string(i) + ".com";
        manager_->CreateTab(url);
    }

    Workspace* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), static_cast<size_t>(NUM_TABS));

    // Last tab should be active
    EXPECT_EQ(ws->active_tab_index, NUM_TABS - 1);
}

TEST_F(TabManagerIntegrationTest, ManyWorkspaces) {
    const int NUM_WORKSPACES = 10;

    for (int i = 2; i <= NUM_WORKSPACES; ++i) {
        std::string name = "WS " + std::to_string(i);
        manager_->CreateWorkspace(name);
    }

    EXPECT_EQ(manager_->GetWorkspaces().size(), static_cast<size_t>(NUM_WORKSPACES));
}

TEST_F(TabManagerIntegrationTest, RapidTabCreateClose) {
    SetupCallbacks();

    std::vector<int> tabIds;
    for (int i = 0; i < 20; ++i) {
        Tab* tab = manager_->CreateTab("https://test.google.com");
        tabIds.push_back(tab->id);
    }

    // Close every other tab
    for (size_t i = 0; i < tabIds.size(); i += 2) {
        manager_->CloseTab(tabIds[i]);
    }

    Workspace* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 10u);

    // All created callbacks should have fired
    EXPECT_EQ(created_tabs_.size(), 20u);

    // 10 close callbacks should have fired
    EXPECT_EQ(closed_tabs_.size(), 10u);
}
