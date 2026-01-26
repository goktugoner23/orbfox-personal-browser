#include "download_manager.h"
#include "gtest/gtest.h"

// Test fixture for DownloadManager tests
// Note: DownloadManager is a singleton, so we need to be careful about state
class DownloadManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        manager_ = &DownloadManager::GetInstance();
        // Clear any existing state
        ClearAllDownloads();
    }

    void TearDown() override {
        // Clean up after each test
        ClearAllDownloads();
    }

    void ClearAllDownloads() {
        // Get all downloads and remove them one by one
        auto downloads = manager_->GetDownloads();
        for (const auto& d : downloads) {
            manager_->RemoveDownload(d.id);
        }
    }

    DownloadItem CreateTestDownload(uint32_t id, DownloadState state,
                                    int64_t total_bytes = 1000,
                                    int64_t received_bytes = 500) {
        DownloadItem item;
        item.id = id;
        item.url = "https://example.com/file" + std::to_string(id) + ".zip";
        item.original_url = item.url;
        item.filename = "file" + std::to_string(id) + ".zip";
        item.total_bytes = total_bytes;
        item.received_bytes = received_bytes;
        item.percent_complete = (total_bytes > 0) ?
            static_cast<int>(100 * received_bytes / total_bytes) : -1;
        item.state = state;
        item.start_time = std::time(nullptr) - 60;  // Started 1 minute ago
        return item;
    }

    DownloadManager* manager_ = nullptr;
};

// ============================================================================
// GetOverallProgress Tests
// ============================================================================

TEST_F(DownloadManagerTest, GetOverallProgress_NoDownloads) {
    // No downloads should return -1.0f
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), -1.0f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_SingleComplete) {
    auto item = CreateTestDownload(1, DownloadState::Complete, 1000, 1000);
    manager_->UpdateDownload(item);

    // Completed downloads are not active, should return -1.0f
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), -1.0f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_SingleInProgress_50Percent) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);
    manager_->UpdateDownload(item);

    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.5f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_SingleInProgress_0Percent) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 0);
    manager_->UpdateDownload(item);

    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.0f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_SingleInProgress_100Percent) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 1000);
    manager_->UpdateDownload(item);

    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 1.0f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_MultipleInProgress) {
    auto item1 = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);  // 50%
    auto item2 = CreateTestDownload(2, DownloadState::InProgress, 2000, 1000); // 50%
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    // Total: 3000 bytes, received: 1500 bytes = 50%
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.5f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_MixedProgress) {
    auto item1 = CreateTestDownload(1, DownloadState::InProgress, 1000, 1000); // 100%
    auto item2 = CreateTestDownload(2, DownloadState::InProgress, 1000, 0);    // 0%
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    // Total: 2000 bytes, received: 1000 bytes = 50%
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.5f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_UnknownSize) {
    // Unknown size (total_bytes = -1 or 0)
    auto item = CreateTestDownload(1, DownloadState::InProgress, 0, 500);
    manager_->UpdateDownload(item);

    // Should return -2.0f to indicate unknown size
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), -2.0f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_MixedKnownUnknownSize) {
    auto item1 = CreateTestDownload(1, DownloadState::InProgress, 1000, 500); // Known
    auto item2 = CreateTestDownload(2, DownloadState::InProgress, 0, 200);    // Unknown
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    // Only known-size downloads counted: 1000 total, 500 received = 50%
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.5f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_Paused) {
    auto item = CreateTestDownload(1, DownloadState::Paused, 1000, 300);
    manager_->UpdateDownload(item);

    // Paused downloads count as active
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), 0.3f);
}

TEST_F(DownloadManagerTest, GetOverallProgress_Canceled_NotCounted) {
    auto item = CreateTestDownload(1, DownloadState::Canceled, 1000, 500);
    manager_->UpdateDownload(item);

    // Canceled downloads are not active
    EXPECT_FLOAT_EQ(manager_->GetOverallProgress(), -1.0f);
}

// ============================================================================
// HasActiveDownloads Tests
// ============================================================================

TEST_F(DownloadManagerTest, HasActiveDownloads_Empty) {
    EXPECT_FALSE(manager_->HasActiveDownloads());
}

TEST_F(DownloadManagerTest, HasActiveDownloads_InProgress) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);
    EXPECT_TRUE(manager_->HasActiveDownloads());
}

TEST_F(DownloadManagerTest, HasActiveDownloads_Paused) {
    auto item = CreateTestDownload(1, DownloadState::Paused);
    manager_->UpdateDownload(item);
    EXPECT_TRUE(manager_->HasActiveDownloads());
}

TEST_F(DownloadManagerTest, HasActiveDownloads_Complete) {
    auto item = CreateTestDownload(1, DownloadState::Complete);
    manager_->UpdateDownload(item);
    EXPECT_FALSE(manager_->HasActiveDownloads());
}

TEST_F(DownloadManagerTest, HasActiveDownloads_Canceled) {
    auto item = CreateTestDownload(1, DownloadState::Canceled);
    manager_->UpdateDownload(item);
    EXPECT_FALSE(manager_->HasActiveDownloads());
}

// ============================================================================
// UpdateDownload Tests
// ============================================================================

TEST_F(DownloadManagerTest, UpdateDownload_NewItem) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].id, 1u);
    EXPECT_EQ(downloads[0].state, DownloadState::InProgress);
}

TEST_F(DownloadManagerTest, UpdateDownload_ExistingItem) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 100);
    manager_->UpdateDownload(item);

    // Update progress
    item.received_bytes = 500;
    item.percent_complete = 50;
    manager_->UpdateDownload(item);

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].received_bytes, 500);
}

TEST_F(DownloadManagerTest, UpdateDownload_PreservesStartTime) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    item.start_time = 12345;
    manager_->UpdateDownload(item);

    // Update with start_time = 0 (should preserve original)
    item.start_time = 0;
    item.received_bytes = 600;
    manager_->UpdateDownload(item);

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].start_time, 12345);
}

TEST_F(DownloadManagerTest, UpdateDownload_PreservesOriginalUrl) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    item.original_url = "https://original.com/file.zip";
    manager_->UpdateDownload(item);

    // Update with empty original_url (should preserve)
    item.original_url = "";
    manager_->UpdateDownload(item);

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].original_url, "https://original.com/file.zip");
}

// ============================================================================
// GetDownloads Sorting Tests
// ============================================================================

TEST_F(DownloadManagerTest, GetDownloads_SortedByStartTime) {
    auto item1 = CreateTestDownload(1, DownloadState::InProgress);
    item1.start_time = 100;
    auto item2 = CreateTestDownload(2, DownloadState::InProgress);
    item2.start_time = 200;
    auto item3 = CreateTestDownload(3, DownloadState::InProgress);
    item3.start_time = 150;

    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);
    manager_->UpdateDownload(item3);

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 3u);

    // Most recent first (descending order)
    EXPECT_EQ(downloads[0].id, 2u);  // start_time 200
    EXPECT_EQ(downloads[1].id, 3u);  // start_time 150
    EXPECT_EQ(downloads[2].id, 1u);  // start_time 100
}

// ============================================================================
// GetDownload by ID Tests
// ============================================================================

TEST_F(DownloadManagerTest, GetDownload_Found) {
    auto item = CreateTestDownload(42, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    auto* result = manager_->GetDownload(42);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id, 42u);
}

TEST_F(DownloadManagerTest, GetDownload_NotFound) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    auto* result = manager_->GetDownload(999);
    EXPECT_EQ(result, nullptr);
}

// ============================================================================
// Cancel/Pause/Resume Tests
// ============================================================================

TEST_F(DownloadManagerTest, CancelDownload_UpdatesState) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    manager_->CancelDownload(1);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Canceled);
    EXPECT_GT(result->end_time, 0);
}

TEST_F(DownloadManagerTest, PauseDownload_InProgress) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    manager_->PauseDownload(1);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Paused);
}

TEST_F(DownloadManagerTest, PauseDownload_NotInProgress_NoChange) {
    auto item = CreateTestDownload(1, DownloadState::Complete);
    manager_->UpdateDownload(item);

    manager_->PauseDownload(1);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Complete);  // Unchanged
}

TEST_F(DownloadManagerTest, ResumeDownload_Paused) {
    auto item = CreateTestDownload(1, DownloadState::Paused);
    manager_->UpdateDownload(item);

    manager_->ResumeDownload(1);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::InProgress);
}

TEST_F(DownloadManagerTest, ResumeDownload_NotPaused_NoChange) {
    auto item = CreateTestDownload(1, DownloadState::Canceled);
    manager_->UpdateDownload(item);

    manager_->ResumeDownload(1);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Canceled);  // Unchanged
}

// ============================================================================
// RemoveDownload Tests
// ============================================================================

TEST_F(DownloadManagerTest, RemoveDownload_Exists) {
    auto item = CreateTestDownload(1, DownloadState::Complete);
    manager_->UpdateDownload(item);

    ASSERT_EQ(manager_->GetDownloads().size(), 1u);

    manager_->RemoveDownload(1);

    EXPECT_EQ(manager_->GetDownloads().size(), 0u);
}

TEST_F(DownloadManagerTest, RemoveDownload_NotExists) {
    auto item = CreateTestDownload(1, DownloadState::Complete);
    manager_->UpdateDownload(item);

    // Remove non-existent ID
    manager_->RemoveDownload(999);

    // Original item still exists
    EXPECT_EQ(manager_->GetDownloads().size(), 1u);
}

// ============================================================================
// ClearCompleted Tests
// ============================================================================

TEST_F(DownloadManagerTest, ClearCompleted_RemovesComplete) {
    auto item1 = CreateTestDownload(1, DownloadState::Complete);
    auto item2 = CreateTestDownload(2, DownloadState::InProgress);
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    manager_->ClearCompleted();

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].id, 2u);
}

TEST_F(DownloadManagerTest, ClearCompleted_RemovesCanceled) {
    auto item1 = CreateTestDownload(1, DownloadState::Canceled);
    auto item2 = CreateTestDownload(2, DownloadState::InProgress);
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    manager_->ClearCompleted();

    auto downloads = manager_->GetDownloads();
    ASSERT_EQ(downloads.size(), 1u);
    EXPECT_EQ(downloads[0].id, 2u);
}

TEST_F(DownloadManagerTest, ClearCompleted_KeepsInProgress) {
    auto item1 = CreateTestDownload(1, DownloadState::InProgress);
    auto item2 = CreateTestDownload(2, DownloadState::Paused);
    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    manager_->ClearCompleted();

    // Both should remain
    EXPECT_EQ(manager_->GetDownloads().size(), 2u);
}

// ============================================================================
// Pending Original URL Tests
// ============================================================================

TEST_F(DownloadManagerTest, PendingOriginalUrl_SetAndGet) {
    manager_->SetPendingOriginalUrl("https://original.com/file.zip");

    std::string url = manager_->GetAndClearPendingOriginalUrl();
    EXPECT_EQ(url, "https://original.com/file.zip");

    // Should be cleared after get
    url = manager_->GetAndClearPendingOriginalUrl();
    EXPECT_EQ(url, "");
}

// ============================================================================
// IsRestart Flag Tests
// ============================================================================

TEST_F(DownloadManagerTest, IsRestart_SetAndGet) {
    manager_->SetIsRestart(true);

    bool result = manager_->GetAndClearIsRestart();
    EXPECT_TRUE(result);

    // Should be cleared after get
    result = manager_->GetAndClearIsRestart();
    EXPECT_FALSE(result);
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST_F(DownloadManagerTest, StateTransition_InProgressToComplete) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);
    manager_->UpdateDownload(item);

    item.state = DownloadState::Complete;
    item.received_bytes = 1000;
    item.percent_complete = 100;
    item.end_time = std::time(nullptr);
    manager_->UpdateDownload(item);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Complete);
    EXPECT_EQ(result->received_bytes, 1000);
}

TEST_F(DownloadManagerTest, StateTransition_InProgressToInterrupted) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);
    manager_->UpdateDownload(item);

    item.state = DownloadState::Interrupted;
    manager_->UpdateDownload(item);

    auto* result = manager_->GetDownload(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->state, DownloadState::Interrupted);
}
