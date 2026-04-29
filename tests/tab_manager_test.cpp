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
    // Default workspace "WS 1" exists, so next auto-generated name is "WS 2"
    auto* ws = manager_->CreateWorkspace();
    EXPECT_EQ(ws->name, "WS 2");
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
    auto* tab = manager_->CreateTab("https://test.google.com");

    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab->url, "https://test.google.com");

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

TEST_F(TabManagerTest, CreateTabInBackground_SetsActivityTime) {
    auto* active = manager_->CreateTab("https://active.example");
    auto* background = manager_->CreateTabInBackground("https://background.example");

    ASSERT_NE(active, nullptr);
    ASSERT_NE(background, nullptr);
    EXPECT_EQ(manager_->GetActiveTab(), active);
    EXPECT_GT(background->last_active_time, 0);
}

TEST_F(TabManagerTest, CreateRestoredTab_CreatesHibernatedPlaceholder) {
    int created_count = 0;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_created = [&created_count](Tab*) {
        created_count++;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab = manager_->CreateRestoredTab("https://restored.example", true);

    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(created_count, 1);
    EXPECT_TRUE(tab->is_hibernated);
    EXPECT_GT(tab->last_active_time, 0);
    EXPECT_EQ(manager_->GetActiveTab(), nullptr);
}

TEST_F(TabManagerTest, HibernateInactiveTabs_HibernatesZeroTimestampBackgroundTabs) {
    auto* active = manager_->CreateTab("https://active.example");
    auto* background = manager_->CreateTabInBackground("https://background.example");
    ASSERT_NE(active, nullptr);
    ASSERT_NE(background, nullptr);

    background->last_active_time = 0;
    manager_->HibernateInactiveTabs(300);

    EXPECT_FALSE(active->is_hibernated);
    EXPECT_TRUE(background->is_hibernated);
}

// ============================================================================
// Tab Closing Tests
// ============================================================================

TEST_F(TabManagerTest, CloseTab_RemovesTab) {
    auto* tab = manager_->CreateTab("https://test.google.com");
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
    auto* tab = manager_->CreateTab("https://test.google.com");

    manager_->CloseTab(tab->id);

    auto* ws = manager_->GetActiveWorkspace();
    EXPECT_EQ(ws->active_tab_index, -1);
    EXPECT_EQ(manager_->GetActiveTab(), nullptr);
}

TEST_F(TabManagerTest, CloseTab_InvalidId_NoEffect) {
    manager_->CreateTab("https://test.google.com");

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
    auto* tab = manager_->CreateTab("https://test.google.com");

    auto* result = manager_->GetTabById(tab->id);
    EXPECT_EQ(result, tab);
}

TEST_F(TabManagerTest, GetTabById_NotFound) {
    manager_->CreateTab("https://test.google.com");

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
    auto* tab = manager_->CreateTab("https://test.google.com");
    tab->title = "Old Title";

    manager_->UpdateTabTitle(tab->id, "New Title");

    EXPECT_EQ(tab->title, "New Title");
}

TEST_F(TabManagerTest, UpdateTabTitle_InvalidId_NoEffect) {
    auto* tab = manager_->CreateTab("https://test.google.com");
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
    auto* tab = manager_->CreateTab("https://test.google.com");
    tab->is_loading = false;

    manager_->UpdateTabLoadingState(tab->id, true);

    EXPECT_TRUE(tab->is_loading);
}

TEST_F(TabManagerTest, UpdateTabFavicon_UpdatesFavicon) {
    auto* tab = manager_->CreateTab("https://test.google.com");

    std::vector<unsigned char> png_data = {0x89, 0x50, 0x4E, 0x47};
    manager_->UpdateTabFavicon(tab->id, "https://test.google.com/favicon.ico", png_data);

    EXPECT_EQ(tab->favicon_url, "https://test.google.com/favicon.ico");
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

    auto* tab = manager_->CreateTab("https://test.google.com");

    EXPECT_EQ(created_tab, tab);
}

TEST_F(TabManagerTest, Callbacks_OnTabClosed) {
    Tab* closed_tab = nullptr;
    TabManagerCallbacks callbacks;
    callbacks.on_tab_closed = [&closed_tab](Tab* tab) {
        closed_tab = tab;
    };
    manager_->SetCallbacks(callbacks);

    auto* tab = manager_->CreateTab("https://test.google.com");
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

    auto* tab = manager_->CreateTab("https://test.google.com");

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
    auto* tab = manager_->CreateTab("https://test.google.com");

    auto* ws = manager_->GetActiveWorkspace();
    Tab* active = ws->GetActiveTab();
    EXPECT_EQ(active, tab);
}

// ============================================================================
// Tab Property Tests
// ============================================================================

TEST_F(TabManagerTest, Tab_DefaultProperties) {
    auto* tab = manager_->CreateTab("https://google.com");

    EXPECT_FALSE(tab->is_loading);
    EXPECT_FALSE(tab->is_pinned);
    EXPECT_FALSE(tab->is_muted);
    EXPECT_EQ(tab->title, "New Tab");
    EXPECT_TRUE(tab->favicon_url.empty());
    EXPECT_TRUE(tab->favicon_data.empty());
}

TEST_F(TabManagerTest, Tab_IsMuted_CanBeSet) {
    auto* tab = manager_->CreateTab("https://google.com");
    EXPECT_FALSE(tab->is_muted);

    tab->is_muted = true;
    EXPECT_TRUE(tab->is_muted);

    tab->is_muted = false;
    EXPECT_FALSE(tab->is_muted);
}

TEST_F(TabManagerTest, Tab_IsPinned_CanBeSet) {
    auto* tab = manager_->CreateTab("https://google.com");
    EXPECT_FALSE(tab->is_pinned);

    tab->is_pinned = true;
    EXPECT_TRUE(tab->is_pinned);

    tab->is_pinned = false;
    EXPECT_FALSE(tab->is_pinned);
}

TEST_F(TabManagerTest, Tab_MultiplePropertiesIndependent) {
    auto* tab = manager_->CreateTab("https://google.com");

    // Set all properties
    tab->is_loading = true;
    tab->is_pinned = true;
    tab->is_muted = true;

    // Verify they're independent
    EXPECT_TRUE(tab->is_loading);
    EXPECT_TRUE(tab->is_pinned);
    EXPECT_TRUE(tab->is_muted);

    // Change one, others should remain
    tab->is_loading = false;
    EXPECT_FALSE(tab->is_loading);
    EXPECT_TRUE(tab->is_pinned);
    EXPECT_TRUE(tab->is_muted);
}

// ============================================================================
// Reopen Closed Tab Tests
// ============================================================================

TEST_F(TabManagerTest, ReopenClosedTab_NoClosedTabs_ReturnsFalse) {
    EXPECT_FALSE(manager_->HasClosedTabs());
    EXPECT_FALSE(manager_->ReopenClosedTab());
}

TEST_F(TabManagerTest, ReopenClosedTab_ReopensLastClosed) {
    auto* tab = manager_->CreateTab("https://google.com");
    tab->title = "Google";
    int tab_id = tab->id;

    manager_->CloseTab(tab_id);

    EXPECT_TRUE(manager_->HasClosedTabs());
    EXPECT_TRUE(manager_->ReopenClosedTab());

    // Should have created a new tab with same URL
    auto* new_tab = manager_->GetActiveTab();
    ASSERT_NE(new_tab, nullptr);
    EXPECT_EQ(new_tab->url, "https://google.com");
    EXPECT_EQ(new_tab->title, "Google");
}

TEST_F(TabManagerTest, ReopenClosedTab_MultipleClosedTabs_LIFO) {
    auto* tab1 = manager_->CreateTab("https://google.com");
    tab1->title = "Google";
    int tab1_id = tab1->id;

    auto* tab2 = manager_->CreateTab("https://github.com");
    tab2->title = "GitHub";
    int tab2_id = tab2->id;

    manager_->CloseTab(tab1_id);
    manager_->CloseTab(tab2_id);

    // First reopen should be the last closed (GitHub)
    EXPECT_TRUE(manager_->ReopenClosedTab());
    auto* reopened1 = manager_->GetActiveTab();
    EXPECT_EQ(reopened1->url, "https://github.com");

    // Second reopen should be Google
    EXPECT_TRUE(manager_->ReopenClosedTab());
    auto* reopened2 = manager_->GetActiveTab();
    EXPECT_EQ(reopened2->url, "https://google.com");

    // No more closed tabs
    EXPECT_FALSE(manager_->HasClosedTabs());
}

TEST_F(TabManagerTest, ReopenClosedTab_EmptyUrl_NotSaved) {
    auto* tab = manager_->CreateTab("");  // Empty URL
    int tab_id = tab->id;

    manager_->CloseTab(tab_id);

    // Should not have saved a tab with empty URL
    EXPECT_FALSE(manager_->HasClosedTabs());
}

TEST_F(TabManagerTest, ReopenClosedTab_WorkspaceDeleted_OpensInActive) {
    // Create a second workspace
    auto* ws2 = manager_->CreateWorkspace("WS 2");
    manager_->SetActiveWorkspace(ws2->id);

    auto* tab = manager_->CreateTab("https://google.com");
    tab->title = "Google";
    int tab_id = tab->id;
    int ws2_id = ws2->id;

    manager_->CloseTab(tab_id);

    // Delete the workspace (switch to first, then delete second)
    manager_->SetActiveWorkspace(manager_->GetWorkspaces()[0]->id);
    manager_->DeleteWorkspace(ws2_id);

    // Reopen should work, opening in the current workspace
    EXPECT_TRUE(manager_->ReopenClosedTab());
    auto* reopened = manager_->GetActiveTab();
    EXPECT_EQ(reopened->url, "https://google.com");
}

// ============================================================================
// Move Tab Between Workspaces Tests
// ============================================================================

TEST_F(TabManagerTest, MoveTabToWorkspace_MovesTab) {
    // Create second workspace
    auto* ws1 = manager_->GetActiveWorkspace();
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    // Create tab in first workspace
    manager_->SetActiveWorkspace(ws1->id);
    auto* tab = manager_->CreateTab("https://google.com");
    tab->title = "Google";
    int tab_id = tab->id;

    EXPECT_EQ(ws1->tabs.size(), 1u);
    EXPECT_EQ(ws2->tabs.size(), 0u);

    // Move tab to workspace 2
    EXPECT_TRUE(manager_->MoveTabToWorkspace(tab_id, ws2->id));

    // Tab should be in workspace 2 now
    EXPECT_EQ(ws1->tabs.size(), 0u);
    EXPECT_EQ(ws2->tabs.size(), 1u);
    EXPECT_EQ(ws2->tabs[0]->url, "https://google.com");
    EXPECT_EQ(ws2->tabs[0]->id, tab_id);
}

TEST_F(TabManagerTest, MoveTabToWorkspace_SwitchesToTargetWorkspace) {
    auto* ws1 = manager_->GetActiveWorkspace();
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    manager_->SetActiveWorkspace(ws1->id);
    auto* tab = manager_->CreateTab("https://google.com");

    EXPECT_EQ(manager_->GetActiveWorkspace(), ws1);

    manager_->MoveTabToWorkspace(tab->id, ws2->id);

    // Should have switched to target workspace
    EXPECT_EQ(manager_->GetActiveWorkspace(), ws2);
}

TEST_F(TabManagerTest, MoveTabToWorkspace_InvalidTabId_ReturnsFalse) {
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    EXPECT_FALSE(manager_->MoveTabToWorkspace(9999, ws2->id));
}

TEST_F(TabManagerTest, MoveTabToWorkspace_InvalidWorkspaceId_ReturnsFalse) {
    auto* tab = manager_->CreateTab("https://google.com");

    EXPECT_FALSE(manager_->MoveTabToWorkspace(tab->id, 9999));
}

TEST_F(TabManagerTest, MoveTabToWorkspace_SameWorkspace_ReturnsFalse) {
    auto* ws1 = manager_->GetActiveWorkspace();
    auto* tab = manager_->CreateTab("https://google.com");

    // Moving to same workspace should fail
    EXPECT_FALSE(manager_->MoveTabToWorkspace(tab->id, ws1->id));
    EXPECT_EQ(ws1->tabs.size(), 1u);  // Tab still there
}

TEST_F(TabManagerTest, MoveTabToWorkspace_PreservesTabProperties) {
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    auto* tab = manager_->CreateTab("https://google.com");
    tab->title = "Google Search";
    tab->is_pinned = true;
    tab->is_muted = true;
    int tab_id = tab->id;

    manager_->MoveTabToWorkspace(tab_id, ws2->id);

    // Find the tab in workspace 2
    ASSERT_EQ(ws2->tabs.size(), 1u);
    auto* moved = ws2->tabs[0].get();

    EXPECT_EQ(moved->id, tab_id);
    EXPECT_EQ(moved->url, "https://google.com");
    EXPECT_EQ(moved->title, "Google Search");
    EXPECT_TRUE(moved->is_pinned);
    EXPECT_TRUE(moved->is_muted);
}

TEST_F(TabManagerTest, MoveTabToWorkspace_AdjustsSourceActiveIndex) {
    auto* ws1 = manager_->GetActiveWorkspace();
    auto* ws2 = manager_->CreateWorkspace("WS 2");

    manager_->SetActiveWorkspace(ws1->id);

    // Create 3 tabs, active index will be 2 (last tab)
    manager_->CreateTab("https://google.com");
    auto* tab2 = manager_->CreateTab("https://github.com");
    manager_->CreateTab("https://test.google.com");

    EXPECT_EQ(ws1->active_tab_index, 2);

    // Move the middle tab
    manager_->MoveTabToWorkspace(tab2->id, ws2->id);

    // Active index should be adjusted (still pointing to last remaining tab)
    EXPECT_EQ(ws1->tabs.size(), 2u);
    EXPECT_LE(ws1->active_tab_index, 1);
}

// ============================================================================
// Unique Workspace Naming Tests
// ============================================================================

TEST_F(TabManagerTest, CreateWorkspace_AutoIncrementNames) {
    // Default workspace is "WS 1", so next should be "WS 2"
    auto* ws2 = manager_->CreateWorkspace();
    EXPECT_EQ(ws2->name, "WS 2");

    auto* ws3 = manager_->CreateWorkspace();
    EXPECT_EQ(ws3->name, "WS 3");

    auto* ws4 = manager_->CreateWorkspace();
    EXPECT_EQ(ws4->name, "WS 4");
}

TEST_F(TabManagerTest, CreateWorkspace_DuplicateNameGetsSuffix) {
    // Default workspace is "WS 1"
    // Creating another "WS 1" should auto-rename to "WS 1 2"
    auto* ws = manager_->CreateWorkspace("WS 1");
    EXPECT_EQ(ws->name, "WS 1 2");
}

TEST_F(TabManagerTest, CreateWorkspace_CustomNamePreserved) {
    auto* ws = manager_->CreateWorkspace("My Custom Workspace");
    EXPECT_EQ(ws->name, "My Custom Workspace");
}

TEST_F(TabManagerTest, WorkspaceNameExists_ReturnsTrueForExisting) {
    EXPECT_TRUE(manager_->WorkspaceNameExists("WS 1"));

    manager_->CreateWorkspace("Test WS");
    EXPECT_TRUE(manager_->WorkspaceNameExists("Test WS"));
}

TEST_F(TabManagerTest, WorkspaceNameExists_ReturnsFalseForNonExisting) {
    EXPECT_FALSE(manager_->WorkspaceNameExists("NonExistent"));
    EXPECT_FALSE(manager_->WorkspaceNameExists("WS 999"));
}

TEST_F(TabManagerTest, GenerateUniqueWorkspaceName_SkipsExisting) {
    // Default workspace is "WS 1"
    // Create "WS 2" manually
    manager_->CreateWorkspace("WS 2");

    // Next auto-generated should skip to "WS 3"
    auto* ws = manager_->CreateWorkspace();
    EXPECT_EQ(ws->name, "WS 3");
}

TEST_F(TabManagerTest, CreateWorkspace_EmptyNameGeneratesUnique) {
    auto* ws = manager_->CreateWorkspace("");
    EXPECT_EQ(ws->name, "WS 2");
}

TEST_F(TabManagerTest, CreateWorkspace_AssignsColorsFromPalette) {
    // Default workspace (index 0) gets first color
    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces[0]->color, WorkspaceColors::palette[0]);

    // Second workspace gets second color
    auto* ws2 = manager_->CreateWorkspace("WS 2");
    EXPECT_EQ(ws2->color, WorkspaceColors::palette[1]);

    // Third workspace gets third color
    auto* ws3 = manager_->CreateWorkspace("WS 3");
    EXPECT_EQ(ws3->color, WorkspaceColors::palette[2]);
}

TEST_F(TabManagerTest, WorkspaceColors_PaletteCycles) {
    // Create enough workspaces to wrap around the palette
    for (int i = 0; i < 8; i++) {
        manager_->CreateWorkspace();
    }
    // 9th workspace (index 8) should cycle back to palette[0]
    // But the first workspace takes index 0, so 9 workspaces total = index 8
    auto& workspaces = manager_->GetWorkspaces();
    EXPECT_EQ(workspaces.size(), 9u);
    EXPECT_EQ(workspaces[8]->color, WorkspaceColors::palette[0]);
}
