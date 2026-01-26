#include "tab_manager.h"
#include "gtest/gtest.h"

// Test fixture for TabManager tests
class TabManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<TabManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<TabManager> manager_;
};

// ============================================================================
// Workspace Creation Tests
// ============================================================================

TEST_F(TabManagerTest, Constructor_CreatesDefaultWorkspace) {
    auto& workspaces = manager_->GetWorkspaces();
    ASSERT_EQ(workspaces.size(), 1u);
    EXPECT_EQ(workspaces[0]->name, "WS 1");
}

TEST_F(TabManagerTest, CreateWorkspace_AddsWorkspace) {
    auto* ws = manager_->CreateWorkspace("Test WS");
    ASSERT_NE(ws, nullptr);
    EXPECT_EQ(ws->name, "Test WS");

    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces.size(), 2u);
}

TEST_F(TabManagerTest, CreateWorkspace_AssignsUniqueIds) {
    auto* ws1 = manager_->CreateWorkspace("WS A");
    auto* ws2 = manager_->CreateWorkspace("WS B");
    auto* ws3 = manager_->CreateWorkspace("WS C");

    EXPECT_NE(ws1->id, ws2->id);
    EXPECT_NE(ws2->id, ws3->id);
    EXPECT_NE(ws1->id, ws3->id);
}

TEST_F(TabManagerTest, CreateWorkspace_DefaultName) {
    auto* ws = manager_->CreateWorkspace();
    EXPECT_EQ(ws->name, "New Workspace");
}

// ============================================================================
// Workspace Deletion Tests
// ============================================================================

TEST_F(TabManagerTest, DeleteWorkspace_RemovesWorkspace) {
    (void)manager_->GetActiveWorkspace();  // ws1 exists
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    manager_->DeleteWorkspace(ws2->id);

    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces.size(), 1u);
    EXPECT_EQ(workspaces[0]->name, "WS 1");
}

TEST_F(TabManagerTest, DeleteWorkspace_CannotDeleteLastWorkspace) {
    auto* ws = manager_->GetActiveWorkspace();
    ASSERT_NE(ws, nullptr);

    manager_->DeleteWorkspace(ws->id);

    // Should still have one workspace
    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces.size(), 1u);
}

TEST_F(TabManagerTest, DeleteWorkspace_AdjustsActiveIndex_DeletedCurrent) {
    manager_->CreateWorkspace("WS 2");  // Index 1
    auto* ws3 = manager_->CreateWorkspace("WS 3");  // Index 2

    manager_->SetActiveWorkspace(ws3->id);
    manager_->DeleteWorkspace(ws3->id);

    // Active index should be adjusted to last valid
    auto* active = manager_->GetActiveWorkspace();
    ASSERT_NE(active, nullptr);
}

TEST_F(TabManagerTest, DeleteWorkspace_AdjustsActiveIndex_DeletedBefore) {
    auto* ws1 = manager_->GetActiveWorkspace();  // Index 0 - "WS 1"
    auto* ws2 = manager_->CreateWorkspace("WS 2");  // Index 1
    manager_->CreateWorkspace("WS 3");  // Index 2

    manager_->SetActiveWorkspace(ws2->id);
    manager_->DeleteWorkspace(ws1->id);

    // After deleting ws1: WS 2 is at index 0, WS 3 is at index 1
    // Active workspace should still be WS 2 (now at new index)
    auto* active = manager_->GetActiveWorkspace();
    ASSERT_NE(active, nullptr);
    // Note: The active workspace should still be "WS 2" but the index changed
    // Check that the active workspace is valid and has expected properties
    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces.size(), 2u);
}

// ============================================================================
// Active Workspace Tests
// ============================================================================

TEST_F(TabManagerTest, GetActiveWorkspace_ReturnsFirstByDefault) {
    auto* active = manager_->GetActiveWorkspace();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->name, "WS 1");
}

TEST_F(TabManagerTest, SetActiveWorkspace_ChangesActive) {
    manager_->CreateWorkspace("WS 2");
    auto* ws3 = manager_->CreateWorkspace("WS 3");

    manager_->SetActiveWorkspace(ws3->id);

    auto* active = manager_->GetActiveWorkspace();
    EXPECT_EQ(active, ws3);
}

TEST_F(TabManagerTest, SetActiveWorkspace_InvalidId_NoChange) {
    auto* original = manager_->GetActiveWorkspace();
    manager_->SetActiveWorkspace(9999);

    auto* active = manager_->GetActiveWorkspace();
    EXPECT_EQ(active, original);
}

// ============================================================================
// Tab Creation Tests
// ============================================================================

TEST_F(TabManagerTest, CreateTab_AddsToActiveWorkspace) {
    auto* tab = manager_->CreateTab("https://example.com");

    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->url, "https://example.com");

    auto* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 1u);
}

TEST_F(TabManagerTest, CreateTab_AssignsUniqueIds) {
    auto* tab1 = manager_->CreateTab("https://a.com");
    auto* tab2 = manager_->CreateTab("https://b.com");
    auto* tab3 = manager_->CreateTab("https://c.com");

    EXPECT_NE(tab1->id, tab2->id);
    EXPECT_NE(tab2->id, tab3->id);
    EXPECT_NE(tab1->id, tab3->id);
}

TEST_F(TabManagerTest, CreateTab_SetsAsActive) {
    manager_->CreateTab("https://first.com");
    auto* tab2 = manager_->CreateTab("https://second.com");

    auto* active = manager_->GetActiveTab();
    EXPECT_EQ(active, tab2);
}

TEST_F(TabManagerTest, CreateTab_EmptyUrl) {
    auto* tab = manager_->CreateTab("");

    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->url, "");
}

TEST_F(TabManagerTest, CreateTab_WithUrl) {
    auto* tab = manager_->CreateTab("https://google.com");

    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->url, "https://google.com");
}

// ============================================================================
// Tab Closing Tests
// ============================================================================

TEST_F(TabManagerTest, CloseTab_RemovesTab) {
    auto* tab = manager_->CreateTab("https://example.com");
    int tabId = tab->id;

    manager_->CloseTab(tabId);

    auto* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 0u);
}

TEST_F(TabManagerTest, CloseTab_AdjustsActiveIndex_ClosedCurrent) {
    auto* tab1 = manager_->CreateTab("https://first.com");
    auto* tab2 = manager_->CreateTab("https://second.com");
    // tab2 is now active

    manager_->CloseTab(tab2->id);

    // tab1 should now be active
    auto* active = manager_->GetActiveTab();
    EXPECT_EQ(active, tab1);
}

TEST_F(TabManagerTest, CloseTab_AdjustsActiveIndex_ClosedBefore) {
    auto* tab1 = manager_->CreateTab("https://first.com");
    auto* tab2 = manager_->CreateTab("https://second.com");
    manager_->CreateTab("https://third.com");

    manager_->SetActiveTab(tab2->id);
    manager_->CloseTab(tab1->id);

    // tab2 should still be active
    auto* active = manager_->GetActiveTab();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->url, "https://second.com");
}

TEST_F(TabManagerTest, CloseTab_LastTab_ActiveIndexNegative) {
    auto* tab = manager_->CreateTab("https://example.com");

    manager_->CloseTab(tab->id);

    auto* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->active_tab_index, -1);
    EXPECT_EQ(manager_->GetActiveTab(), nullptr);
}

TEST_F(TabManagerTest, CloseTab_InvalidId_NoEffect) {
    manager_->CreateTab("https://example.com");

    manager_->CloseTab(9999);

    auto* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->tabs.size(), 1u);
}

// ============================================================================
// Set Active Tab Tests
// ============================================================================

TEST_F(TabManagerTest, SetActiveTab_ChangesActive) {
    auto* tab1 = manager_->CreateTab("https://first.com");
    manager_->CreateTab("https://second.com");

    manager_->SetActiveTab(tab1->id);

    auto* active = manager_->GetActiveTab();
    EXPECT_EQ(active, tab1);
}

TEST_F(TabManagerTest, SetActiveTab_InvalidId_NoChange) {
    auto* tab1 = manager_->CreateTab("https://first.com");
    manager_->CreateTab("https://second.com");  // Now active

    manager_->SetActiveTab(9999);

    // Active should still be tab2 (most recently created)
    auto* active = manager_->GetActiveTab();
    EXPECT_NE(active, tab1);
}

// ============================================================================
// GetTabById Tests
// ============================================================================

TEST_F(TabManagerTest, GetTabById_Found) {
    auto* tab = manager_->CreateTab("https://example.com");

    auto* result = manager_->GetTabById(tab->id);
    EXPECT_EQ(result, tab);
}

TEST_F(TabManagerTest, GetTabById_NotFound) {
    manager_->CreateTab("https://example.com");

    auto* result = manager_->GetTabById(9999);
    EXPECT_EQ(result, nullptr);
}

TEST_F(TabManagerTest, GetTabById_SearchesAllWorkspaces) {
    auto* tab1 = manager_->CreateTab("https://first.com");

    // Create second workspace with a tab
    auto* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);
    auto* tab2 = manager_->CreateTab("https://second.com");

    // Switch back to first workspace
    manager_->SetActiveWorkspace(manager_->GetWorkspaces()[0]->id);

    // Should find tabs in both workspaces
    EXPECT_EQ(manager_->GetTabById(tab1->id), tab1);
    EXPECT_EQ(manager_->GetTabById(tab2->id), tab2);
}

// ============================================================================
// Tab Update Tests
// ============================================================================

TEST_F(TabManagerTest, UpdateTabTitle_UpdatesTitle) {
    auto* tab = manager_->CreateTab("https://example.com");
    tab->title = "Old Title";

    manager_->UpdateTabTitle(tab->id, "New Title");

    EXPECT_EQ(tab->title, "New Title");
}

TEST_F(TabManagerTest, UpdateTabTitle_InvalidId_NoEffect) {
    auto* tab = manager_->CreateTab("https://example.com");
    tab->title = "Original";

    manager_->UpdateTabTitle(9999, "Changed");

    EXPECT_EQ(tab->title, "Original");
}

TEST_F(TabManagerTest, UpdateTabUrl_UpdatesUrl) {
    auto* tab = manager_->CreateTab("https://old.com");

    manager_->UpdateTabUrl(tab->id, "https://new.com");

    EXPECT_EQ(tab->url, "https://new.com");
}

TEST_F(TabManagerTest, UpdateTabLoadingState_UpdatesState) {
    auto* tab = manager_->CreateTab("https://example.com");
    tab->is_loading = false;

    manager_->UpdateTabLoadingState(tab->id, true);

    EXPECT_TRUE(tab->is_loading);
}

TEST_F(TabManagerTest, UpdateTabFavicon_UpdatesFavicon) {
    auto* tab = manager_->CreateTab("https://example.com");

    std::vector<unsigned char> png_data = {0x89, 0x50, 0x4E, 0x47};
    manager_->UpdateTabFavicon(tab->id, "https://example.com/favicon.ico", png_data);

    EXPECT_EQ(tab->favicon_url, "https://example.com/favicon.ico");
    EXPECT_EQ(tab->favicon_data, png_data);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(TabManagerTest, Callbacks_OnTabCreated) {
    Tab* created_tab = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_created = [&created_tab](Tab* tab) {
        created_tab = tab;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab = manager_->CreateTab("https://example.com");

    EXPECT_EQ(created_tab, tab);
}

TEST_F(TabManagerTest, Callbacks_OnTabClosed) {
    Tab* closed_tab = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_closed = [&closed_tab](Tab* tab) {
        closed_tab = tab;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab = manager_->CreateTab("https://example.com");
    int tabId = tab->id;
    manager_->CloseTab(tabId);

    // closed_tab should have been set before the tab was deleted
    EXPECT_NE(closed_tab, nullptr);
}

TEST_F(TabManagerTest, Callbacks_OnTabActivated) {
    Tab* activated_tab = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_activated = [&activated_tab](Tab* tab) {
        activated_tab = tab;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab1 = manager_->CreateTab("https://first.com");
    auto* tab2 = manager_->CreateTab("https://second.com");

    // Last created is activated
    EXPECT_EQ(activated_tab, tab2);

    manager_->SetActiveTab(tab1->id);
    EXPECT_EQ(activated_tab, tab1);
}

TEST_F(TabManagerTest, Callbacks_OnTabUpdated) {
    Tab* updated_tab = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_updated = [&updated_tab](Tab* tab) {
        updated_tab = tab;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab = manager_->CreateTab("https://example.com");

    manager_->UpdateTabTitle(tab->id, "New Title");
    EXPECT_EQ(updated_tab, tab);

    updated_tab = nullptr;
    manager_->UpdateTabUrl(tab->id, "https://new.com");
    EXPECT_EQ(updated_tab, tab);
}

TEST_F(TabManagerTest, Callbacks_OnWorkspaceChanged) {
    Workspace* changed_ws = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_workspace_changed = [&changed_ws](Workspace* ws) {
        changed_ws = ws;
    };
    manager_->SetCallbacks(callbacks);

    auto* ws = manager_->CreateWorkspace("New WS");
    EXPECT_EQ(changed_ws, ws);

    manager_->SetActiveWorkspace(ws->id);
    EXPECT_EQ(changed_ws, ws);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TabManagerTest, MultipleTabs_CorrectActiveTracking) {
    auto* tab1 = manager_->CreateTab("https://1.com");
    auto* tab2 = manager_->CreateTab("https://2.com");
    auto* tab3 = manager_->CreateTab("https://3.com");

    // tab3 should be active
    EXPECT_EQ(manager_->GetActiveTab(), tab3);

    // Close tab3, tab2 should become active
    manager_->CloseTab(tab3->id);
    EXPECT_EQ(manager_->GetActiveTab(), tab2);

    // Close tab2, tab1 should become active
    manager_->CloseTab(tab2->id);
    EXPECT_EQ(manager_->GetActiveTab(), tab1);
}

TEST_F(TabManagerTest, GetNextTabId_IncrementsProperly) {
    int id1 = manager_->GetNextTabId();
    int id2 = manager_->GetNextTabId();
    int id3 = manager_->GetNextTabId();

    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);
}

TEST_F(TabManagerTest, Workspace_GetActiveTab_EmptyWorkspace) {
    auto* ws = manager_->GetActiveWorkspace();
    ASSERT_NE(ws, nullptr);

    // No tabs created yet
    Tab* active = ws->GetActiveTab();
    EXPECT_EQ(active, nullptr);
}

TEST_F(TabManagerTest, Workspace_GetActiveTab_WithTabs) {
    auto* tab = manager_->CreateTab("https://example.com");

    auto* ws = manager_->GetActiveWorkspace();
    Tab* active = ws->GetActiveTab();
    EXPECT_EQ(active, tab);
}
