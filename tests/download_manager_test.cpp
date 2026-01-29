#include "download_manager.h"
#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>
#include <fstream>

// Test fixture for DownloadManager tests
//
// IMPORTANT: DownloadManager is a singleton, which means:
// 1. Tests MUST NOT run in parallel (use sequential test execution)
// 2. Each test must reset state in SetUp/TearDown to avoid leakage
// 3. Tests share the same DownloadManager instance across all test methods
//
// The singleton pattern is intentional here because the download manager
// genuinely needs to be global to track all downloads across the application.
class DownloadManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance and reset all state
        manager_ = &DownloadManager::GetInstance();
        manager_->ResetForTesting();
    }

    void TearDown() override {
        // Clean up after each test
        manager_->ResetForTesting();
    }

    DownloadItem CreateTestDownload(uint32_t id, DownloadState state,
                                    int64_t total_bytes = 1000,
                                    int64_t received_bytes = 500) {
        DownloadItem item;
        item.id = id;
        item.url = "https://test.google.com/file" + std::to_string(id) + ".zip";
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

    auto result = manager_->GetDownload(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 42u);
}

TEST_F(DownloadManagerTest, GetDownload_NotFound) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    auto result = manager_->GetDownload(999);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Cancel/Pause/Resume Tests
// ============================================================================

TEST_F(DownloadManagerTest, CancelDownload_UpdatesState) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    manager_->CancelDownload(1);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::Canceled);
    EXPECT_GT(result->end_time, 0);
}

TEST_F(DownloadManagerTest, PauseDownload_InProgress) {
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    manager_->PauseDownload(1);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::Paused);
}

TEST_F(DownloadManagerTest, PauseDownload_NotInProgress_NoChange) {
    auto item = CreateTestDownload(1, DownloadState::Complete);
    manager_->UpdateDownload(item);

    manager_->PauseDownload(1);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::Complete);  // Unchanged
}

TEST_F(DownloadManagerTest, ResumeDownload_Paused) {
    auto item = CreateTestDownload(1, DownloadState::Paused);
    manager_->UpdateDownload(item);

    manager_->ResumeDownload(1);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::InProgress);
}

TEST_F(DownloadManagerTest, ResumeDownload_NotPaused_NoChange) {
    auto item = CreateTestDownload(1, DownloadState::Canceled);
    manager_->UpdateDownload(item);

    manager_->ResumeDownload(1);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
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

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::Complete);
    EXPECT_EQ(result->received_bytes, 1000);
}

TEST_F(DownloadManagerTest, StateTransition_InProgressToInterrupted) {
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);
    manager_->UpdateDownload(item);

    item.state = DownloadState::Interrupted;
    manager_->UpdateDownload(item);

    auto result = manager_->GetDownload(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, DownloadState::Interrupted);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

#include <thread>
#include <atomic>
#include <chrono>

TEST_F(DownloadManagerTest, ThreadSafety_ConcurrentUpdates) {
    // Test that concurrent updates from multiple threads don't cause data races
    const int num_threads = 4;
    std::atomic<int> completed_threads{0};
    std::vector<std::thread> threads;

    // Create initial downloads (one per thread)
    for (int i = 0; i < num_threads; ++i) {
        auto item = CreateTestDownload(static_cast<uint32_t>(i + 1), DownloadState::InProgress, 10000, 0);
        manager_->UpdateDownload(item);
    }

    // Launch threads that update their own download
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &completed_threads]() {
            uint32_t id = static_cast<uint32_t>(t + 1);
            constexpr int updates_count = 100;
            for (int i = 0; i < updates_count; ++i) {
                DownloadItem item;
                item.id = id;
                item.url = "https://test.google.com/file" + std::to_string(id) + ".zip";
                item.filename = "file" + std::to_string(id) + ".zip";
                item.total_bytes = 10000;
                item.received_bytes = (i + 1) * 100;  // Progress from 100 to 10000
                item.percent_complete = (i + 1);
                item.state = DownloadState::InProgress;
                manager_->UpdateDownload(item);
            }
            ++completed_threads;
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed_threads.load(), num_threads);

    // Verify all downloads are present and have correct final state
    auto downloads = manager_->GetDownloads();
    EXPECT_EQ(downloads.size(), static_cast<size_t>(num_threads));

    for (int i = 0; i < num_threads; ++i) {
        auto result = manager_->GetDownload(static_cast<uint32_t>(i + 1));
        ASSERT_TRUE(result.has_value());
        // Final received_bytes should be 10000 (100 updates * 100)
        EXPECT_EQ(result->received_bytes, 10000);
    }
}

TEST_F(DownloadManagerTest, ThreadSafety_ConcurrentReadsAndWrites) {
    // Test that concurrent reads and writes don't cause data races
    const int num_writers = 2;
    const int num_readers = 4;
    std::atomic<bool> should_stop{false};
    std::atomic<int> read_count{0};
    std::vector<std::thread> threads;

    // Create initial downloads
    for (int i = 1; i <= 10; ++i) {
        auto item = CreateTestDownload(static_cast<uint32_t>(i), DownloadState::InProgress, 1000, 500);
        manager_->UpdateDownload(item);
    }

    // Launch writer threads
    for (int t = 0; t < num_writers; ++t) {
        threads.emplace_back([this]() {
            constexpr int writer_ops = 50;
            for (int i = 0; i < writer_ops; ++i) {
                uint32_t id = static_cast<uint32_t>((i % 10) + 1);
                auto item = CreateTestDownload(id, DownloadState::InProgress, 1000, (i * 10) % 1000);
                manager_->UpdateDownload(item);
            }
        });
    }

    // Launch reader threads
    for (int t = 0; t < num_readers; ++t) {
        threads.emplace_back([this, &should_stop, &read_count]() {
            while (!should_stop.load()) {
                // Test GetDownloads (returns copy)
                auto downloads = manager_->GetDownloads();
                (void)downloads;  // Just read, don't need to verify

                // Test GetDownload (returns std::optional copy)
                for (uint32_t id = 1; id <= 10; ++id) {
                    auto result = manager_->GetDownload(id);
                    if (result.has_value()) {
                        // Verify the copy is valid (not corrupted by concurrent access)
                        EXPECT_GE(result->id, 1u);
                        EXPECT_LE(result->id, 10u);
                    }
                }

                // Test HasActiveDownloads
                (void)manager_->HasActiveDownloads();

                // Test GetOverallProgress
                (void)manager_->GetOverallProgress();

                ++read_count;
            }
        });
    }

    // Wait for writers to finish
    for (int i = 0; i < num_writers; ++i) {
        threads[i].join();
    }

    // Signal readers to stop
    should_stop.store(true);

    // Wait for readers to finish
    for (int i = num_writers; i < static_cast<int>(threads.size()); ++i) {
        threads[i].join();
    }

    // Verify readers completed many iterations
    EXPECT_GT(read_count.load(), 0);
}

TEST_F(DownloadManagerTest, ThreadSafety_GetDownloadReturnsCopy) {
    // Verify that GetDownload returns a copy that is safe to use after the lock is released
    auto item = CreateTestDownload(1, DownloadState::InProgress, 1000, 500);
    item.filename = "original.zip";
    manager_->UpdateDownload(item);

    // Get a copy
    auto copy = manager_->GetDownload(1);
    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(copy->filename, "original.zip");

    // Modify the original in the manager
    item.filename = "modified.zip";
    item.received_bytes = 999;
    manager_->UpdateDownload(item);

    // The copy should still have the old values (it's a copy, not a reference)
    EXPECT_EQ(copy->filename, "original.zip");
    EXPECT_EQ(copy->received_bytes, 500);

    // But getting a new copy should have the updated values
    auto new_copy = manager_->GetDownload(1);
    ASSERT_TRUE(new_copy.has_value());
    EXPECT_EQ(new_copy->filename, "modified.zip");
    EXPECT_EQ(new_copy->received_bytes, 999);
}

TEST_F(DownloadManagerTest, ThreadSafety_ConcurrentAddAndRemove) {
    // Test concurrent addition and removal of downloads
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<uint32_t> next_id{1000};

    // Launch threads that add and remove downloads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &next_id]() {
            constexpr int ops_count = 50;
            std::vector<uint32_t> added_ids;
            for (int i = 0; i < ops_count; ++i) {
                // Add a new download
                uint32_t id = next_id.fetch_add(1);
                auto item = CreateTestDownload(id, DownloadState::Complete, 1000, 1000);
                manager_->UpdateDownload(item);
                added_ids.push_back(id);

                // Occasionally remove some of our own downloads
                if (i > 0 && i % 5 == 0 && !added_ids.empty()) {
                    uint32_t to_remove = added_ids[added_ids.size() / 2];
                    manager_->RemoveDownload(to_remove);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Just verify we didn't crash and data is consistent
    auto downloads = manager_->GetDownloads();
    for (const auto& d : downloads) {
        EXPECT_GE(d.id, 1000u);
    }
}

TEST_F(DownloadManagerTest, ThreadSafety_ConcurrentStateChanges) {
    // Test concurrent pause/resume/cancel operations
    const int num_downloads = 10;

    // Create downloads
    for (uint32_t i = 1; i <= num_downloads; ++i) {
        auto item = CreateTestDownload(i, DownloadState::InProgress, 1000, 500);
        manager_->UpdateDownload(item);
    }

    std::vector<std::thread> threads;

    // Thread that pauses downloads
    threads.emplace_back([this]() {
        for (int round = 0; round < 10; ++round) {
            for (uint32_t i = 1; i <= 10; ++i) {
                manager_->PauseDownload(i);
            }
        }
    });

    // Thread that resumes downloads
    threads.emplace_back([this]() {
        for (int round = 0; round < 10; ++round) {
            for (uint32_t i = 1; i <= 10; ++i) {
                manager_->ResumeDownload(i);
            }
        }
    });

    // Thread that reads states
    threads.emplace_back([this]() {
        for (int round = 0; round < 20; ++round) {
            for (uint32_t i = 1; i <= 10; ++i) {
                auto result = manager_->GetDownload(i);
                if (result.has_value()) {
                    // State should be one of the valid states
                    EXPECT_TRUE(
                        result->state == DownloadState::InProgress ||
                        result->state == DownloadState::Paused ||
                        result->state == DownloadState::Canceled
                    );
                }
            }
        }
    });

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All downloads should still exist
    EXPECT_EQ(manager_->GetDownloads().size(), static_cast<size_t>(num_downloads));
}

TEST_F(DownloadManagerTest, ThreadSafety_CallbackInvokedOutsideLock) {
    // Test that callbacks are invoked outside the lock (no deadlock)
    std::atomic<int> callback_count{0};
    std::atomic<bool> callback_running{false};

    manager_->SetUpdateCallback([this, &callback_count, &callback_running]() {
        callback_running.store(true);
        ++callback_count;

        // Try to call another manager method from within callback
        // This would deadlock if callback was called while holding the lock
        auto downloads = manager_->GetDownloads();
        (void)downloads;

        callback_running.store(false);
    });

    // Update should trigger callback
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    // Verify callback was called
    EXPECT_GT(callback_count.load(), 0);

    // Clear callback
    manager_->SetUpdateCallback(nullptr);
}

TEST_F(DownloadManagerTest, ThreadSafety_PendingUrlAcrossThreads) {
    // Test SetPendingOriginalUrl and GetAndClearPendingOriginalUrl across threads
    const int num_threads = 4;
    const int operations = 100;
    std::vector<std::thread> threads;
    std::atomic<int> set_count{0};
    std::atomic<int> get_count{0};

    // Launch threads that set URLs
    for (int t = 0; t < num_threads / 2; ++t) {
        threads.emplace_back([this, &set_count, t]() {
            constexpr int ops = 100;
            for (int i = 0; i < ops; ++i) {
                std::string url = "https://thread" + std::to_string(t) + "/file" + std::to_string(i);
                manager_->SetPendingOriginalUrl(url);
                ++set_count;
            }
        });
    }

    // Launch threads that get URLs
    for (int t = 0; t < num_threads / 2; ++t) {
        threads.emplace_back([this, &get_count]() {
            constexpr int ops = 100;
            for (int i = 0; i < ops; ++i) {
                std::string url = manager_->GetAndClearPendingOriginalUrl();
                (void)url;  // Just checking for data races
                ++get_count;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(set_count.load(), (num_threads / 2) * operations);
    EXPECT_EQ(get_count.load(), (num_threads / 2) * operations);
}

TEST_F(DownloadManagerTest, ThreadSafety_IsRestartFlagAcrossThreads) {
    // Test SetIsRestart and GetAndClearIsRestart across threads
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> true_results{0};

    // Launch threads that set and get the flag
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &true_results]() {
            constexpr int ops = 100;
            for (int i = 0; i < ops; ++i) {
                manager_->SetIsRestart(true);
                if (manager_->GetAndClearIsRestart()) {
                    ++true_results;
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // We should have gotten true at least once (likely many times)
    EXPECT_GT(true_results.load(), 0);
}

TEST_F(DownloadManagerTest, GetDownload_ReturnsOptionalNotPointer) {
    // Test that GetDownload returns std::optional (thread-safe copy) not pointer
    auto item = CreateTestDownload(1, DownloadState::InProgress);
    manager_->UpdateDownload(item);

    // Get the download
    std::optional<DownloadItem> result = manager_->GetDownload(1);

    // Verify it's an optional
    ASSERT_TRUE(result.has_value());

    // Verify we can copy and use it safely
    DownloadItem copy = *result;
    EXPECT_EQ(copy.id, 1u);

    // Verify non-existent download returns empty optional (not null pointer)
    std::optional<DownloadItem> not_found = manager_->GetDownload(999);
    EXPECT_FALSE(not_found.has_value());
}

// ============================================================================
// Persistence Tests (LoadFromDisk / SaveToDisk)
// ============================================================================

class DownloadManagerPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = &DownloadManager::GetInstance();
        manager_->ResetForTesting();

        // Create unique test directory
        test_dir_ = "/tmp/orbfox_download_test_" +
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        test_file_ = test_dir_ + "/downloads.json";
    }

    void TearDown() override {
        manager_->ResetForTesting();
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }

    DownloadItem CreateCompletedDownload(uint32_t id) {
        DownloadItem item;
        item.id = id;
        item.url = "https://test.google.com/file" + std::to_string(id) + ".zip";
        item.original_url = item.url;
        item.filename = "file" + std::to_string(id) + ".zip";
        item.full_path = "/Users/test/Downloads/" + item.filename;
        item.mime_type = "application/zip";
        item.total_bytes = 1000;
        item.received_bytes = 1000;
        item.percent_complete = 100;
        item.state = DownloadState::Complete;
        item.start_time = std::time(nullptr) - 60;
        item.end_time = std::time(nullptr);
        return item;
    }

    DownloadManager* manager_ = nullptr;
    std::string test_dir_;
    std::string test_file_;
};

TEST_F(DownloadManagerPersistenceTest, SaveToDisk_CreatesFile) {
    auto item = CreateCompletedDownload(1);
    manager_->UpdateDownload(item);

    manager_->SaveToDisk(test_file_);

    EXPECT_TRUE(std::filesystem::exists(test_file_));
}

TEST_F(DownloadManagerPersistenceTest, SaveToDisk_OnlyCompletedDownloads) {
    // Add various download states
    auto completed = CreateCompletedDownload(1);

    DownloadItem in_progress;
    in_progress.id = 2;
    in_progress.url = "https://test.google.com/inprogress.zip";
    in_progress.state = DownloadState::InProgress;

    DownloadItem canceled;
    canceled.id = 3;
    canceled.url = "https://test.google.com/canceled.zip";
    canceled.state = DownloadState::Canceled;

    manager_->UpdateDownload(completed);
    manager_->UpdateDownload(in_progress);
    manager_->UpdateDownload(canceled);

    manager_->SaveToDisk(test_file_);

    // Reset and load
    manager_->ResetForTesting();
    manager_->LoadFromDisk(test_file_);

    auto downloads = manager_->GetDownloads();

    // Should have 2 downloads (completed and canceled, not in_progress)
    EXPECT_EQ(downloads.size(), 2u);
}

TEST_F(DownloadManagerPersistenceTest, LoadFromDisk_RestoresDownloads) {
    auto item1 = CreateCompletedDownload(1);
    auto item2 = CreateCompletedDownload(2);
    item2.filename = "other_file.zip";

    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);
    manager_->SaveToDisk(test_file_);

    // Reset state
    manager_->ResetForTesting();
    EXPECT_EQ(manager_->GetDownloads().size(), 0u);

    // Load from disk
    manager_->LoadFromDisk(test_file_);

    auto downloads = manager_->GetDownloads();
    EXPECT_EQ(downloads.size(), 2u);
}

TEST_F(DownloadManagerPersistenceTest, LoadFromDisk_RestoresAllFields) {
    auto original = CreateCompletedDownload(42);
    original.url = "https://test.google.com/testfile.zip";
    original.original_url = "https://redirect.google.com/testfile.zip";
    original.filename = "testfile.zip";
    original.full_path = "/Users/test/Downloads/testfile.zip";
    original.mime_type = "application/zip";
    original.total_bytes = 5000;
    original.received_bytes = 5000;
    original.percent_complete = 100;

    manager_->UpdateDownload(original);
    manager_->SaveToDisk(test_file_);

    manager_->ResetForTesting();
    manager_->LoadFromDisk(test_file_);

    auto result = manager_->GetDownload(42);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->id, 42u);
    EXPECT_EQ(result->url, "https://test.google.com/testfile.zip");
    EXPECT_EQ(result->original_url, "https://redirect.google.com/testfile.zip");
    EXPECT_EQ(result->filename, "testfile.zip");
    EXPECT_EQ(result->full_path, "/Users/test/Downloads/testfile.zip");
    EXPECT_EQ(result->mime_type, "application/zip");
    EXPECT_EQ(result->total_bytes, 5000);
    EXPECT_EQ(result->received_bytes, 5000);
    EXPECT_EQ(result->percent_complete, 100);
    EXPECT_EQ(result->state, DownloadState::Complete);
}

TEST_F(DownloadManagerPersistenceTest, LoadFromDisk_NonexistentFile_NoEffect) {
    // Add a download
    auto item = CreateCompletedDownload(1);
    manager_->UpdateDownload(item);

    // Try to load from nonexistent file
    manager_->LoadFromDisk("/nonexistent/path/downloads.json");

    // Original download should still be there
    EXPECT_EQ(manager_->GetDownloads().size(), 1u);
}

TEST_F(DownloadManagerPersistenceTest, LoadFromDisk_EmptyFile_ClearsDownloads) {
    // Add a download
    auto item = CreateCompletedDownload(1);
    manager_->UpdateDownload(item);

    // Create empty file
    std::ofstream empty_file(test_file_);
    empty_file << "[]";
    empty_file.close();

    // Load from empty file
    manager_->LoadFromDisk(test_file_);

    // Downloads should be cleared
    EXPECT_EQ(manager_->GetDownloads().size(), 0u);
}

TEST_F(DownloadManagerPersistenceTest, SaveLoadRoundtrip_PreservesData) {
    // Create multiple downloads with different states
    auto item1 = CreateCompletedDownload(1);

    DownloadItem item2;
    item2.id = 2;
    item2.url = "https://test.google.com/interrupted.zip";
    item2.original_url = item2.url;
    item2.filename = "interrupted.zip";
    item2.state = DownloadState::Interrupted;
    item2.total_bytes = 2000;
    item2.received_bytes = 500;

    manager_->UpdateDownload(item1);
    manager_->UpdateDownload(item2);

    // Save
    manager_->SaveToDisk(test_file_);

    // Reset and reload
    manager_->ResetForTesting();
    manager_->LoadFromDisk(test_file_);

    // Verify
    auto downloads = manager_->GetDownloads();
    EXPECT_EQ(downloads.size(), 2u);

    auto d1 = manager_->GetDownload(1);
    auto d2 = manager_->GetDownload(2);

    ASSERT_TRUE(d1.has_value());
    ASSERT_TRUE(d2.has_value());

    EXPECT_EQ(d1->state, DownloadState::Complete);
    EXPECT_EQ(d2->state, DownloadState::Interrupted);
}
