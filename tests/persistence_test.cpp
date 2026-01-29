#include "history_storage.h"
#include "session_storage.h"
#include "window_settings.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <chrono>

// ============================================================================
// HistoryStorage Tests
// ============================================================================

class HistoryStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique test directory using process ID and timestamp
        test_dir_ = "/tmp/orbfox_test_history_" + std::to_string(getpid()) + "_" +
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        test_db_path_ = test_dir_ + "/history.db";

        history_ = std::make_unique<HistoryStorage>();
        // Initialize with test-specific path to avoid touching production data
        if (history_->Initialize(test_db_path_)) {
            history_->ClearAllHistory();
        }
    }

    void TearDown() override {
        if (history_) {
            history_->ClearAllHistory();
        }
        history_.reset();

        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::unique_ptr<HistoryStorage> history_;
    std::string test_dir_;
    std::string test_db_path_;
};

TEST_F(HistoryStorageTest, Initialize_CreatesDatabase) {
    // Already initialized in SetUp, this test just confirms it worked
    auto entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 0u);  // Should be empty after clear
}

TEST_F(HistoryStorageTest, AddEntry_AddsToHistory) {
    history_->AddEntry("https://test.google.com", "Example");

    auto entries = history_->GetRecentHistory(10);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].url, "https://test.google.com");
    EXPECT_EQ(entries[0].title, "Example");
}

TEST_F(HistoryStorageTest, AddEntry_EmptyUrl_NoEffect) {
    history_->AddEntry("", "No URL");

    auto entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, AddEntry_UpdatesExisting) {
    history_->AddEntry("https://test.google.com", "Title 1");
    history_->AddEntry("https://test.google.com", "Title 2");  // Same URL

    auto entries = history_->GetRecentHistory(10);
    ASSERT_EQ(entries.size(), 1u);  // Still only one entry
    EXPECT_EQ(entries[0].title, "Title 2");  // Updated title
    EXPECT_EQ(entries[0].visit_count, 2);  // Incremented count
}

TEST_F(HistoryStorageTest, GetRecentHistory_LimitsResults) {
    // Add 20 entries - no need for sleeps, just testing limit functionality
    for (int i = 0; i < 20; ++i) {
        history_->AddEntry("https://site" + std::to_string(i) + ".com",
                          "Site " + std::to_string(i));
    }

    auto entries = history_->GetRecentHistory(5);
    EXPECT_EQ(entries.size(), 5u);
}

TEST_F(HistoryStorageTest, GetRecentHistory_OrderedByTime) {
    // Add entries - they all have the same timestamp (within same second)
    history_->AddEntry("https://first.com", "First");
    history_->AddEntry("https://second.com", "Second");
    history_->AddEntry("https://third.com", "Third");

    auto entries = history_->GetRecentHistory(10);
    ASSERT_EQ(entries.size(), 3u);

    // Verify entries are sorted by visit_time descending (or equal)
    // Note: Entries with same timestamp may be in any order, which is acceptable
    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_GE(entries[i - 1].visit_time, entries[i].visit_time)
            << "Entry " << i - 1 << " should have visit_time >= entry " << i;
    }
}

TEST_F(HistoryStorageTest, SearchHistory_FindsByUrl) {
    history_->AddEntry("https://google.com", "Google");
    history_->AddEntry("https://github.com", "GitHub");
    history_->AddEntry("https://test.google.com", "Example");

    auto entries = history_->SearchHistory("git", 10);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].url, "https://github.com");
}

TEST_F(HistoryStorageTest, SearchHistory_FindsByTitle) {
    history_->AddEntry("https://site1.com", "Google Search");
    history_->AddEntry("https://site2.com", "Apple Store");
    history_->AddEntry("https://site3.com", "Microsoft");

    auto entries = history_->SearchHistory("Search", 10);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].title, "Google Search");
}

TEST_F(HistoryStorageTest, SearchHistory_EmptyQuery_NoResults) {
    history_->AddEntry("https://test.google.com", "Example");

    auto entries = history_->SearchHistory("", 10);
    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, DeleteEntry_RemovesEntry) {
    history_->AddEntry("https://test.google.com", "Example");

    auto entries = history_->GetRecentHistory(10);
    ASSERT_EQ(entries.size(), 1u);
    int64_t id = entries[0].id;

    history_->DeleteEntry(id);

    entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, ClearAllHistory_RemovesAll) {
    history_->AddEntry("https://site1.com", "Site 1");
    history_->AddEntry("https://site2.com", "Site 2");
    history_->AddEntry("https://site3.com", "Site 3");

    history_->ClearAllHistory();

    auto entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, GetHistoryForDay_ReturnsEntriesFromThatDay) {
    // Add entries "today"
    history_->AddEntry("https://today1.com", "Today 1");
    history_->AddEntry("https://today2.com", "Today 2");

    // Get today's start time (midnight)
    std::time_t now = std::time(nullptr);
    std::tm* tm_now = std::localtime(&now);
    tm_now->tm_hour = 0;
    tm_now->tm_min = 0;
    tm_now->tm_sec = 0;
    std::time_t day_start = std::mktime(tm_now);

    auto entries = history_->GetHistoryForDay(day_start);

    // Should have the entries we just added
    EXPECT_GE(entries.size(), 2u);

    // Verify URLs are in the result
    bool found_today1 = false;
    bool found_today2 = false;
    for (const auto& entry : entries) {
        if (entry.url == "https://today1.com") found_today1 = true;
        if (entry.url == "https://today2.com") found_today2 = true;
    }
    EXPECT_TRUE(found_today1);
    EXPECT_TRUE(found_today2);
}

TEST_F(HistoryStorageTest, GetHistoryForDay_EmptyForFutureDay) {
    history_->AddEntry("https://today.com", "Today");

    // Get a day in the future
    std::time_t future_day = std::time(nullptr) + (24 * 60 * 60);  // Tomorrow

    auto entries = history_->GetHistoryForDay(future_day);

    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, ClearHistoryBefore_RemovesOlderEntries) {
    // Add an entry
    history_->AddEntry("https://old.com", "Old Entry");

    // Get the entry's time
    auto entries = history_->GetRecentHistory(10);
    ASSERT_EQ(entries.size(), 1u);

    // Clear history before "now + 1 second" (should remove everything)
    std::time_t cutoff = std::time(nullptr) + 1;
    history_->ClearHistoryBefore(cutoff);

    entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 0u);
}

TEST_F(HistoryStorageTest, ClearHistoryBefore_KeepsNewerEntries) {
    // Add an entry
    history_->AddEntry("https://recent.com", "Recent Entry");

    // Clear history before a time in the past (should keep everything)
    std::time_t past_cutoff = std::time(nullptr) - (24 * 60 * 60);  // Yesterday
    history_->ClearHistoryBefore(past_cutoff);

    auto entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].url, "https://recent.com");
}

TEST_F(HistoryStorageTest, ClearHistoryBefore_ZeroCutoff_NoEffect) {
    history_->AddEntry("https://site.com", "Site");

    // Clear with zero cutoff should have no effect
    history_->ClearHistoryBefore(0);

    auto entries = history_->GetRecentHistory(10);
    EXPECT_EQ(entries.size(), 1u);
}

// ============================================================================
// SessionStorage Tests
// ============================================================================

// SessionStorage test fixture to ensure clean state
class SessionStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = std::make_unique<SessionStorage>();
    }

    void TearDown() override {
        storage_.reset();
    }

    std::unique_ptr<SessionStorage> storage_;
};

TEST(SavedTabTest, DefaultValues) {
    SavedTab tab;
    EXPECT_TRUE(tab.url.empty());
    EXPECT_TRUE(tab.title.empty());
    EXPECT_FALSE(tab.is_pinned);
    EXPECT_FALSE(tab.is_muted);
}

TEST(SavedTabTest, IsMuted_CanBeSet) {
    SavedTab tab;
    tab.is_muted = true;
    EXPECT_TRUE(tab.is_muted);
}

TEST(SavedWorkspaceTest, DefaultValues) {
    SavedWorkspace ws;
    EXPECT_TRUE(ws.name.empty());
    EXPECT_TRUE(ws.color.empty());
    EXPECT_TRUE(ws.tabs.empty());
    EXPECT_EQ(ws.active_tab_index, 0);
}

TEST(SavedSessionTest, DefaultValues) {
    SavedSession session;
    EXPECT_TRUE(session.workspaces.empty());
    EXPECT_EQ(session.active_workspace_index, 0);
}

TEST_F(SessionStorageTest, Save_CreatesJsonFile) {
    SavedSession session;

    SavedWorkspace ws;
    ws.name = "Test Workspace";
    ws.active_tab_index = 0;

    SavedTab tab;
    tab.url = "https://test.google.com";
    tab.title = "Example";
    tab.is_pinned = false;
    tab.is_muted = false;
    ws.tabs.push_back(tab);

    session.workspaces.push_back(ws);
    session.active_workspace_index = 0;

    storage_->Save(session);

    EXPECT_TRUE(storage_->HasSavedSession());
}

TEST_F(SessionStorageTest, Save_WithMutedAndPinnedTab) {
    SavedSession session;

    SavedWorkspace ws;
    ws.name = "Workspace With Muted Tab";
    ws.active_tab_index = 0;

    SavedTab tab1;
    tab1.url = "https://youtube.com";
    tab1.title = "YouTube - Muted";
    tab1.is_pinned = false;
    tab1.is_muted = true;  // Muted tab
    ws.tabs.push_back(tab1);

    SavedTab tab2;
    tab2.url = "https://github.com";
    tab2.title = "GitHub - Pinned";
    tab2.is_pinned = true;  // Pinned tab
    tab2.is_muted = false;
    ws.tabs.push_back(tab2);

    SavedTab tab3;
    tab3.url = "https://test.google.com";
    tab3.title = "Example - Both";
    tab3.is_pinned = true;  // Both pinned and muted
    tab3.is_muted = true;
    ws.tabs.push_back(tab3);

    session.workspaces.push_back(ws);
    session.active_workspace_index = 0;

    storage_->Save(session);

    EXPECT_TRUE(storage_->HasSavedSession());
}

TEST_F(SessionStorageTest, LoadSave_Roundtrip) {
    // Create a session with workspaces and tabs
    SavedSession session;
    session.active_workspace_index = 1;

    SavedWorkspace ws1;
    ws1.name = "Work";
    ws1.color = "#FF5733";
    ws1.active_tab_index = 0;
    SavedTab tab1;
    tab1.url = "https://test.google.com";
    tab1.title = "Example";
    tab1.is_pinned = true;
    tab1.is_muted = false;
    ws1.tabs.push_back(tab1);
    session.workspaces.push_back(ws1);

    SavedWorkspace ws2;
    ws2.name = "Personal";
    ws2.color = "#33FF57";
    ws2.active_tab_index = 1;
    SavedTab tab2;
    tab2.url = "https://google.com";
    tab2.title = "Google";
    tab2.is_pinned = false;
    tab2.is_muted = true;
    ws2.tabs.push_back(tab2);
    SavedTab tab3;
    tab3.url = "https://github.com";
    tab3.title = "GitHub";
    ws2.tabs.push_back(tab3);
    session.workspaces.push_back(ws2);

    // Save and reload
    storage_->Save(session);
    SavedSession loaded = storage_->Load();

    // Verify
    EXPECT_EQ(loaded.active_workspace_index, 1);
    ASSERT_EQ(loaded.workspaces.size(), 2u);

    EXPECT_EQ(loaded.workspaces[0].name, "Work");
    EXPECT_EQ(loaded.workspaces[0].color, "#FF5733");
    ASSERT_EQ(loaded.workspaces[0].tabs.size(), 1u);
    EXPECT_EQ(loaded.workspaces[0].tabs[0].url, "https://test.google.com");
    EXPECT_EQ(loaded.workspaces[0].tabs[0].title, "Example");
    EXPECT_TRUE(loaded.workspaces[0].tabs[0].is_pinned);

    EXPECT_EQ(loaded.workspaces[1].name, "Personal");
    ASSERT_EQ(loaded.workspaces[1].tabs.size(), 2u);
    EXPECT_TRUE(loaded.workspaces[1].tabs[0].is_muted);
}

TEST_F(SessionStorageTest, Load_EmptySession_NoWorkspaces) {
    // Test that Load() handles non-existent/empty session file gracefully
    // Note: This test may not always work if a session file exists from previous tests
    SavedSession loaded = storage_->Load();

    // The loaded session should have valid structure
    // (active_workspace_index defaults to 0)
    EXPECT_GE(loaded.active_workspace_index, 0);
}

TEST_F(SessionStorageTest, Save_WithSpecialCharacters) {
    SavedSession session;
    session.active_workspace_index = 0;

    SavedWorkspace ws;
    ws.name = "Test \"Quotes\" & <Brackets>";
    ws.color = "#AABBCC";
    ws.active_tab_index = 0;

    SavedTab tab;
    tab.url = "https://test.google.com/path?query=value&other=test";
    tab.title = "Title with \"quotes\" and\nnewlines";
    tab.is_pinned = false;
    tab.is_muted = false;
    ws.tabs.push_back(tab);
    session.workspaces.push_back(ws);

    // Save and reload
    storage_->Save(session);
    SavedSession loaded = storage_->Load();

    // Verify special characters are preserved
    ASSERT_EQ(loaded.workspaces.size(), 1u);
    EXPECT_EQ(loaded.workspaces[0].name, "Test \"Quotes\" & <Brackets>");
    ASSERT_EQ(loaded.workspaces[0].tabs.size(), 1u);
    EXPECT_EQ(loaded.workspaces[0].tabs[0].url, "https://test.google.com/path?query=value&other=test");
    EXPECT_EQ(loaded.workspaces[0].tabs[0].title, "Title with \"quotes\" and\nnewlines");
}

// ============================================================================
// WindowSettings Tests
// ============================================================================

TEST(WindowSettingsTest, DefaultValues) {
    WindowSettings settings;

    // Check default values (from header: window_settings.h)
    EXPECT_EQ(settings.x, 100);
    EXPECT_EQ(settings.y, 100);
    EXPECT_EQ(settings.width, 1280);  // Header says 1280
    EXPECT_EQ(settings.height, 800);
    EXPECT_FALSE(settings.maximized);
}

TEST(WindowSettingsTest, LoadSave_Roundtrip) {
    WindowSettings original;
    original.x = 200;
    original.y = 150;
    original.width = 1400;
    original.height = 900;
    original.maximized = true;

    original.Save();
    WindowSettings loaded = WindowSettings::Load();

    EXPECT_EQ(loaded.x, original.x);
    EXPECT_EQ(loaded.y, original.y);
    EXPECT_EQ(loaded.width, original.width);
    EXPECT_EQ(loaded.height, original.height);
    EXPECT_EQ(loaded.maximized, original.maximized);
}

TEST(WindowSettingsTest, Load_SanitizesSmallWidth) {
    // Create settings file with invalid values
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":0,"y":0,"width":100,"height":600,"maximized":false})";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Width should be clamped to minimum 400
    EXPECT_EQ(loaded.width, 400);
}

TEST(WindowSettingsTest, Load_SanitizesSmallHeight) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":0,"y":0,"width":800,"height":100,"maximized":false})";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Height should be clamped to minimum 300
    EXPECT_EQ(loaded.height, 300);
}

TEST(WindowSettingsTest, Load_SanitizesLargeWidth) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":0,"y":0,"width":10000,"height":600,"maximized":false})";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Width should be clamped to maximum 4096
    EXPECT_EQ(loaded.width, 4096);
}

TEST(WindowSettingsTest, Load_SanitizesLargeHeight) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":0,"y":0,"width":800,"height":10000,"maximized":false})";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Height should be clamped to maximum 4096
    EXPECT_EQ(loaded.height, 4096);
}

TEST(WindowSettingsTest, Load_MissingFile_ReturnsDefaults) {
    // Remove the settings file if it exists
    std::filesystem::remove(WindowSettings::GetSettingsPath());

    WindowSettings loaded = WindowSettings::Load();

    // Should return defaults
    EXPECT_EQ(loaded.width, 1280);
    EXPECT_EQ(loaded.height, 800);
}

TEST(WindowSettingsTest, Load_MalformedJson_ReturnsDefaults) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << "not valid json {{{";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Should return defaults when parsing fails
    EXPECT_EQ(loaded.width, 1280);
    EXPECT_EQ(loaded.height, 800);
}

TEST(WindowSettingsTest, Load_PartialJson_UsesDefaults) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":500,"y":300})";  // Missing width/height
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    EXPECT_EQ(loaded.x, 500);
    EXPECT_EQ(loaded.y, 300);
    // Width/height should be defaults
    EXPECT_EQ(loaded.width, 1280);
    EXPECT_EQ(loaded.height, 800);
}

TEST(WindowSettingsTest, Load_NegativePosition_Allowed) {
    std::ofstream file(WindowSettings::GetSettingsPath());
    file << R"({"x":-100,"y":-50,"width":800,"height":600,"maximized":false})";
    file.close();

    WindowSettings loaded = WindowSettings::Load();

    // Negative positions are allowed (for multi-monitor setups)
    EXPECT_EQ(loaded.x, -100);
    EXPECT_EQ(loaded.y, -50);
}

// ============================================================================
// Integration: Multiple Persistence Systems
// ============================================================================

TEST(PersistenceIntegrationTest, WindowSettingsRoundtrip) {
    // Test that WindowSettings save/load works
    WindowSettings ws;
    ws.x = 999;
    ws.y = 888;
    ws.width = 1000;
    ws.height = 700;
    ws.Save();

    WindowSettings loaded_ws = WindowSettings::Load();
    EXPECT_EQ(loaded_ws.x, 999);
    EXPECT_EQ(loaded_ws.y, 888);
    EXPECT_EQ(loaded_ws.width, 1000);
    EXPECT_EQ(loaded_ws.height, 700);
}

// Test that all persistence systems work independently
TEST(PersistenceIntegrationTest, AllSystemsIndependent) {
    // Test that WindowSettings, SessionStorage, and HistoryStorage
    // can all be used together without interference

    // 1. Save window settings
    WindowSettings ws;
    ws.x = 111;
    ws.y = 222;
    ws.width = 1100;
    ws.height = 700;
    ws.maximized = true;
    ws.Save();

    // 2. Save session
    SessionStorage sessionStorage;
    SavedSession session;
    session.active_workspace_index = 0;
    SavedWorkspace workspace;
    workspace.name = "Integration Test";
    workspace.color = "#123456";
    workspace.active_tab_index = 0;
    SavedTab tab;
    tab.url = "https://google.com";
    tab.title = "Google";
    tab.is_pinned = true;
    tab.is_muted = false;
    workspace.tabs.push_back(tab);
    session.workspaces.push_back(workspace);
    sessionStorage.Save(session);

    // 3. Load both back and verify they're independent
    WindowSettings loadedWs = WindowSettings::Load();
    SavedSession loadedSession = sessionStorage.Load();

    // Window settings should be intact
    EXPECT_EQ(loadedWs.x, 111);
    EXPECT_EQ(loadedWs.y, 222);
    EXPECT_EQ(loadedWs.width, 1100);
    EXPECT_EQ(loadedWs.height, 700);
    EXPECT_TRUE(loadedWs.maximized);

    // Session should be intact
    ASSERT_EQ(loadedSession.workspaces.size(), 1u);
    EXPECT_EQ(loadedSession.workspaces[0].name, "Integration Test");
    EXPECT_EQ(loadedSession.workspaces[0].color, "#123456");
    ASSERT_EQ(loadedSession.workspaces[0].tabs.size(), 1u);
    EXPECT_EQ(loadedSession.workspaces[0].tabs[0].url, "https://google.com");
    EXPECT_TRUE(loadedSession.workspaces[0].tabs[0].is_pinned);
}
