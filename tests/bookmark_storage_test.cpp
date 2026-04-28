#include "bookmark_storage.h"
#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

// ============================================================================
// BookmarkStorage Tests
// ============================================================================

class BookmarkStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique test directory using process ID and timestamp
#ifdef _WIN32
        test_dir_ = std::filesystem::temp_directory_path().string() + "/orbfox_test_bookmarks_" + std::to_string(_getpid()) + "_" +
#else
        test_dir_ = "/tmp/orbfox_test_bookmarks_" + std::to_string(getpid()) + "_" +
#endif
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        test_db_path_ = test_dir_ + "/bookmarks.db";

        storage_ = std::make_unique<BookmarkStorage>();
        // Initialize with test-specific path to avoid touching production data
        if (storage_->Initialize(test_db_path_)) {
            storage_->ClearAllBookmarks();
        }
    }

    void TearDown() override {
        if (storage_) {
            storage_->ClearAllBookmarks();
        }
        storage_.reset();

        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::unique_ptr<BookmarkStorage> storage_;
    std::string test_dir_;
    std::string test_db_path_;

    void AddBookmark(const std::string& url, const std::string& title, const std::string& folder = "") {
        EXPECT_GT(storage_->AddBookmark(url, title, folder), 0);
    }

    void CreateFolder(const std::string& name) {
        EXPECT_TRUE(storage_->CreateFolder(name));
    }
};

// ============================================================================
// Initialize Tests
// ============================================================================

TEST_F(BookmarkStorageTest, Initialize_CreatesDatabase) {
    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

// ============================================================================
// AddBookmark Tests
// ============================================================================

TEST_F(BookmarkStorageTest, AddBookmark_AddsToStorage) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example");

    EXPECT_GT(id, 0);

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 1u);
    EXPECT_EQ(bookmarks[0].url, "https://test.google.com");
    EXPECT_EQ(bookmarks[0].title, "Example");
}

TEST_F(BookmarkStorageTest, AddBookmark_EmptyUrl_ReturnsZero) {
    int64_t id = storage_->AddBookmark("", "No URL");

    EXPECT_EQ(id, 0);

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, AddBookmark_WithFolder) {
    AddBookmark("https://work.com", "Work Site", "Work");

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 1u);
    EXPECT_EQ(bookmarks[0].folder, "Work");
}

TEST_F(BookmarkStorageTest, AddBookmark_DuplicateUrl_UpdatesTitleAndFolder) {
    AddBookmark("https://test.google.com", "Original Title", "");
    AddBookmark("https://test.google.com", "Updated Title", "NewFolder");

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 1u);  // Still only one bookmark
    EXPECT_EQ(bookmarks[0].title, "Updated Title");
    EXPECT_EQ(bookmarks[0].folder, "NewFolder");
}

TEST_F(BookmarkStorageTest, AddBookmark_SetsCreatedTime) {
    std::time_t before = std::time(nullptr);
    AddBookmark("https://test.google.com", "Example");
    std::time_t after = std::time(nullptr);

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 1u);
    EXPECT_GE(bookmarks[0].created_time, before);
    EXPECT_LE(bookmarks[0].created_time, after);
}

TEST_F(BookmarkStorageTest, AddBookmark_AssignsIncrementingPositions) {
    AddBookmark("https://site1.com", "Site 1");
    AddBookmark("https://site2.com", "Site 2");
    AddBookmark("https://site3.com", "Site 3");

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 3u);

    // Positions should be incrementing
    EXPECT_LT(bookmarks[0].position, bookmarks[1].position);
    EXPECT_LT(bookmarks[1].position, bookmarks[2].position);
}

// ============================================================================
// IsBookmarked Tests
// ============================================================================

TEST_F(BookmarkStorageTest, IsBookmarked_ExistingUrl_ReturnsTrue) {
    AddBookmark("https://test.google.com", "Example");

    EXPECT_TRUE(storage_->IsBookmarked("https://test.google.com"));
}

TEST_F(BookmarkStorageTest, IsBookmarked_NonexistentUrl_ReturnsFalse) {
    EXPECT_FALSE(storage_->IsBookmarked("https://notbookmarked.com"));
}

TEST_F(BookmarkStorageTest, IsBookmarked_EmptyUrl_ReturnsFalse) {
    EXPECT_FALSE(storage_->IsBookmarked(""));
}

TEST_F(BookmarkStorageTest, IsBookmarked_AfterDelete_ReturnsFalse) {
    AddBookmark("https://test.google.com", "Example");
    EXPECT_TRUE(storage_->IsBookmarked("https://test.google.com"));

    storage_->DeleteBookmarkByUrl("https://test.google.com");
    EXPECT_FALSE(storage_->IsBookmarked("https://test.google.com"));
}

// ============================================================================
// GetBookmarkByUrl Tests
// ============================================================================

TEST_F(BookmarkStorageTest, GetBookmarkByUrl_Exists_ReturnsBookmark) {
    AddBookmark("https://test.google.com", "Example", "MyFolder");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");

    ASSERT_TRUE(bookmark.has_value());
    EXPECT_GT(bookmark->id, 0);
    EXPECT_EQ(bookmark->url, "https://test.google.com");
    EXPECT_EQ(bookmark->title, "Example");
    EXPECT_EQ(bookmark->folder, "MyFolder");
}

TEST_F(BookmarkStorageTest, GetBookmarkByUrl_NotExists_ReturnsNullopt) {
    auto bookmark = storage_->GetBookmarkByUrl("https://notfound.com");

    EXPECT_FALSE(bookmark.has_value());
}

TEST_F(BookmarkStorageTest, GetBookmarkByUrl_EmptyUrl_ReturnsNullopt) {
    auto bookmark = storage_->GetBookmarkByUrl("");

    EXPECT_FALSE(bookmark.has_value());
}

// ============================================================================
// GetAllBookmarks Tests
// ============================================================================

TEST_F(BookmarkStorageTest, GetAllBookmarks_Empty_ReturnsEmptyVector) {
    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, GetAllBookmarks_MultipleBookmarks_ReturnsAll) {
    AddBookmark("https://site1.com", "Site 1");
    AddBookmark("https://site2.com", "Site 2");
    AddBookmark("https://site3.com", "Site 3");

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 3u);
}

TEST_F(BookmarkStorageTest, GetAllBookmarks_OrderedByFolderThenPosition) {
    AddBookmark("https://b-folder.com", "B Folder", "B");
    AddBookmark("https://a-folder.com", "A Folder", "A");
    AddBookmark("https://root.com", "Root", "");

    auto bookmarks = storage_->GetAllBookmarks();
    ASSERT_EQ(bookmarks.size(), 3u);

    // Empty folder comes first (alphabetically before "A")
    EXPECT_EQ(bookmarks[0].folder, "");
    EXPECT_EQ(bookmarks[1].folder, "A");
    EXPECT_EQ(bookmarks[2].folder, "B");
}

// ============================================================================
// GetBookmarksInFolder Tests
// ============================================================================

TEST_F(BookmarkStorageTest, GetBookmarksInFolder_EmptyFolder_ReturnsRootBookmarks) {
    AddBookmark("https://root1.com", "Root 1", "");
    AddBookmark("https://root2.com", "Root 2", "");
    AddBookmark("https://work.com", "Work", "Work");

    auto bookmarks = storage_->GetBookmarksInFolder("");
    ASSERT_EQ(bookmarks.size(), 2u);
    EXPECT_EQ(bookmarks[0].url, "https://root1.com");
    EXPECT_EQ(bookmarks[1].url, "https://root2.com");
}

TEST_F(BookmarkStorageTest, GetBookmarksInFolder_SpecificFolder_ReturnsOnlyThatFolder) {
    AddBookmark("https://root.com", "Root", "");
    AddBookmark("https://work1.com", "Work 1", "Work");
    AddBookmark("https://work2.com", "Work 2", "Work");
    AddBookmark("https://personal.com", "Personal", "Personal");

    auto bookmarks = storage_->GetBookmarksInFolder("Work");
    ASSERT_EQ(bookmarks.size(), 2u);
    EXPECT_EQ(bookmarks[0].folder, "Work");
    EXPECT_EQ(bookmarks[1].folder, "Work");
}

TEST_F(BookmarkStorageTest, GetBookmarksInFolder_NonexistentFolder_ReturnsEmpty) {
    AddBookmark("https://test.google.com", "Example", "");

    auto bookmarks = storage_->GetBookmarksInFolder("DoesNotExist");
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, GetBookmarksInFolder_OrderedByPosition) {
    AddBookmark("https://first.com", "First", "Work");
    AddBookmark("https://second.com", "Second", "Work");
    AddBookmark("https://third.com", "Third", "Work");

    auto bookmarks = storage_->GetBookmarksInFolder("Work");
    ASSERT_EQ(bookmarks.size(), 3u);

    // Should be ordered by position (insertion order)
    EXPECT_EQ(bookmarks[0].url, "https://first.com");
    EXPECT_EQ(bookmarks[1].url, "https://second.com");
    EXPECT_EQ(bookmarks[2].url, "https://third.com");
}

// ============================================================================
// GetFolders Tests
// ============================================================================

TEST_F(BookmarkStorageTest, GetFolders_NoBookmarks_ReturnsEmpty) {
    auto folders = storage_->GetFolders();
    EXPECT_EQ(folders.size(), 0u);
}

TEST_F(BookmarkStorageTest, GetFolders_OnlyRootBookmarks_ReturnsEmpty) {
    AddBookmark("https://test.google.com", "Example", "");

    auto folders = storage_->GetFolders();
    EXPECT_EQ(folders.size(), 0u);  // Empty folder not included
}

TEST_F(BookmarkStorageTest, GetFolders_MultipleBookmarksInSameFolder_ReturnsUnique) {
    AddBookmark("https://site1.com", "Site 1", "Work");
    AddBookmark("https://site2.com", "Site 2", "Work");
    AddBookmark("https://site3.com", "Site 3", "Work");

    auto folders = storage_->GetFolders();
    ASSERT_EQ(folders.size(), 1u);
    EXPECT_EQ(folders[0], "Work");
}

TEST_F(BookmarkStorageTest, GetFolders_MultipleFolders_ReturnsAllSorted) {
    AddBookmark("https://work.com", "Work", "Work");
    AddBookmark("https://personal.com", "Personal", "Personal");
    AddBookmark("https://archive.com", "Archive", "Archive");

    auto folders = storage_->GetFolders();
    ASSERT_EQ(folders.size(), 3u);

    // Should be sorted alphabetically
    EXPECT_EQ(folders[0], "Archive");
    EXPECT_EQ(folders[1], "Personal");
    EXPECT_EQ(folders[2], "Work");
}

// ============================================================================
// UpdateBookmark Tests
// ============================================================================

TEST_F(BookmarkStorageTest, UpdateBookmark_UpdatesTitle) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Original", "");

    storage_->UpdateBookmark(id, "Updated Title", "");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->title, "Updated Title");
}

TEST_F(BookmarkStorageTest, UpdateBookmark_UpdatesFolder) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example", "");

    storage_->UpdateBookmark(id, "Example", "NewFolder");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->folder, "NewFolder");
}

TEST_F(BookmarkStorageTest, UpdateBookmark_InvalidId_NoEffect) {
    AddBookmark("https://test.google.com", "Example", "");

    storage_->UpdateBookmark(99999, "New Title", "NewFolder");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->title, "Example");  // Unchanged
    EXPECT_EQ(bookmark->folder, "");         // Unchanged
}

// ============================================================================
// DeleteBookmark Tests
// ============================================================================

TEST_F(BookmarkStorageTest, DeleteBookmark_ById_RemovesBookmark) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example");

    storage_->DeleteBookmark(id);

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, DeleteBookmark_InvalidId_NoEffect) {
    AddBookmark("https://test.google.com", "Example");

    storage_->DeleteBookmark(99999);  // Non-existent ID

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 1u);  // Still there
}

TEST_F(BookmarkStorageTest, DeleteBookmarkByUrl_RemovesBookmark) {
    AddBookmark("https://test.google.com", "Example");

    storage_->DeleteBookmarkByUrl("https://test.google.com");

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, DeleteBookmarkByUrl_NonexistentUrl_NoEffect) {
    AddBookmark("https://test.google.com", "Example");

    storage_->DeleteBookmarkByUrl("https://other.com");

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 1u);  // Still there
}

TEST_F(BookmarkStorageTest, DeleteBookmarkByUrl_EmptyUrl_NoEffect) {
    AddBookmark("https://test.google.com", "Example");

    storage_->DeleteBookmarkByUrl("");

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 1u);  // Still there
}

// ============================================================================
// MoveBookmark Tests
// ============================================================================

TEST_F(BookmarkStorageTest, MoveBookmark_ChangesFolder) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example", "OldFolder");

    storage_->MoveBookmark(id, "NewFolder", 1);

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->folder, "NewFolder");
}

TEST_F(BookmarkStorageTest, MoveBookmark_ChangesPosition) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example", "Folder");

    storage_->MoveBookmark(id, "Folder", 999);

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->position, 999);
}

TEST_F(BookmarkStorageTest, MoveBookmark_FromRootToFolder) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example", "");

    storage_->MoveBookmark(id, "Work", 1);

    auto root = storage_->GetBookmarksInFolder("");
    auto work = storage_->GetBookmarksInFolder("Work");

    EXPECT_EQ(root.size(), 0u);
    ASSERT_EQ(work.size(), 1u);
    EXPECT_EQ(work[0].url, "https://test.google.com");
}

TEST_F(BookmarkStorageTest, MoveBookmark_FromFolderToRoot) {
    int64_t id = storage_->AddBookmark("https://test.google.com", "Example", "Work");

    storage_->MoveBookmark(id, "", 1);

    auto root = storage_->GetBookmarksInFolder("");
    auto work = storage_->GetBookmarksInFolder("Work");

    ASSERT_EQ(root.size(), 1u);
    EXPECT_EQ(work.size(), 0u);
    EXPECT_EQ(root[0].url, "https://test.google.com");
}

TEST_F(BookmarkStorageTest, MoveBookmark_InvalidId_NoEffect) {
    AddBookmark("https://test.google.com", "Example", "OldFolder");

    storage_->MoveBookmark(99999, "NewFolder", 1);

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->folder, "OldFolder");  // Unchanged
}

// ============================================================================
// ClearAllBookmarks Tests
// ============================================================================

TEST_F(BookmarkStorageTest, ClearAllBookmarks_RemovesAll) {
    AddBookmark("https://site1.com", "Site 1");
    AddBookmark("https://site2.com", "Site 2");
    AddBookmark("https://site3.com", "Site 3", "Work");

    storage_->ClearAllBookmarks();

    auto bookmarks = storage_->GetAllBookmarks();
    EXPECT_EQ(bookmarks.size(), 0u);
}

TEST_F(BookmarkStorageTest, ClearAllBookmarks_AlsoClaresFolders) {
    AddBookmark("https://work.com", "Work", "Work");
    AddBookmark("https://personal.com", "Personal", "Personal");

    storage_->ClearAllBookmarks();

    auto folders = storage_->GetFolders();
    EXPECT_EQ(folders.size(), 0u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BookmarkStorageTest, SpecialCharactersInTitle) {
    AddBookmark("https://test.google.com", "Test <>&\"' Special Chars");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->title, "Test <>&\"' Special Chars");
}

TEST_F(BookmarkStorageTest, UnicodeInTitle) {
    AddBookmark("https://test.google.com", "Test \xC3\xA9\xC3\xB1\xC3\xBC Unicode");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->title, "Test \xC3\xA9\xC3\xB1\xC3\xBC Unicode");
}

TEST_F(BookmarkStorageTest, LongUrl) {
    std::string long_url = "https://test.google.com/";
    for (int i = 0; i < 100; ++i) {
        long_url += "segment" + std::to_string(i) + "/";
    }

    AddBookmark(long_url, "Long URL");

    auto bookmark = storage_->GetBookmarkByUrl(long_url);
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_EQ(bookmark->url, long_url);
}

TEST_F(BookmarkStorageTest, EmptyTitle) {
    AddBookmark("https://test.google.com", "");

    auto bookmark = storage_->GetBookmarkByUrl("https://test.google.com");
    ASSERT_TRUE(bookmark.has_value());
    EXPECT_TRUE(bookmark->title.empty());
}

TEST_F(BookmarkStorageTest, MultipleOperationsSequence) {
    // Add
    int64_t id1 = storage_->AddBookmark("https://site1.com", "Site 1");
    int64_t id2 = storage_->AddBookmark("https://site2.com", "Site 2");

    // Update
    storage_->UpdateBookmark(id1, "Updated Site 1", "Work");

    // Move
    storage_->MoveBookmark(id2, "Personal", 10);

    // Verify
    auto all = storage_->GetAllBookmarks();
    ASSERT_EQ(all.size(), 2u);

    auto s1 = storage_->GetBookmarkByUrl("https://site1.com");
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(s1->title, "Updated Site 1");
    EXPECT_EQ(s1->folder, "Work");

    auto s2 = storage_->GetBookmarkByUrl("https://site2.com");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->folder, "Personal");
    EXPECT_EQ(s2->position, 10);

    // Delete one
    storage_->DeleteBookmark(id1);

    all = storage_->GetAllBookmarks();
    EXPECT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].url, "https://site2.com");
}

// ============================================================================
// Folder Management Tests
// ============================================================================

TEST_F(BookmarkStorageTest, CreateFolder_CreatesEmptyFolder) {
    bool created = storage_->CreateFolder("My Collection");
    EXPECT_TRUE(created);

    auto folders = storage_->GetFolders();
    ASSERT_EQ(folders.size(), 1u);
    EXPECT_EQ(folders[0], "My Collection");
}

TEST_F(BookmarkStorageTest, CreateFolder_EmptyName_ReturnsFalse) {
    bool created = storage_->CreateFolder("");
    EXPECT_FALSE(created);

    auto folders = storage_->GetFolders();
    EXPECT_EQ(folders.size(), 0u);
}

TEST_F(BookmarkStorageTest, CreateFolder_DuplicateName_ReturnsFalse) {
    CreateFolder("Work");
    bool created = storage_->CreateFolder("Work");
    EXPECT_FALSE(created);

    auto folders = storage_->GetFolders();
    EXPECT_EQ(folders.size(), 1u);
}

TEST_F(BookmarkStorageTest, FolderExists_ExistingEmptyFolder_ReturnsTrue) {
    CreateFolder("Projects");
    EXPECT_TRUE(storage_->FolderExists("Projects"));
}

TEST_F(BookmarkStorageTest, FolderExists_FolderFromBookmarks_ReturnsTrue) {
    AddBookmark("https://test.google.com", "Example", "Work");
    EXPECT_TRUE(storage_->FolderExists("Work"));
}

TEST_F(BookmarkStorageTest, FolderExists_NonexistentFolder_ReturnsFalse) {
    EXPECT_FALSE(storage_->FolderExists("NonExistent"));
}

TEST_F(BookmarkStorageTest, FolderExists_EmptyName_ReturnsFalse) {
    EXPECT_FALSE(storage_->FolderExists(""));
}

TEST_F(BookmarkStorageTest, DeleteFolder_RemovesFolderAndBookmarks) {
    CreateFolder("ToDelete");
    AddBookmark("https://site1.com", "Site 1", "ToDelete");
    AddBookmark("https://site2.com", "Site 2", "ToDelete");
    AddBookmark("https://other.com", "Other", "");  // Root

    storage_->DeleteFolder("ToDelete");

    EXPECT_FALSE(storage_->FolderExists("ToDelete"));

    auto all = storage_->GetAllBookmarks();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].url, "https://other.com");
}

TEST_F(BookmarkStorageTest, DeleteFolder_EmptyName_NoEffect) {
    CreateFolder("Folder1");
    storage_->DeleteFolder("");

    EXPECT_TRUE(storage_->FolderExists("Folder1"));
}

TEST_F(BookmarkStorageTest, DeleteFolder_NonexistentFolder_NoEffect) {
    CreateFolder("Existing");
    storage_->DeleteFolder("NonExistent");

    EXPECT_TRUE(storage_->FolderExists("Existing"));
}

TEST_F(BookmarkStorageTest, RenameFolder_RenamesBookmarksAndFolder) {
    CreateFolder("OldName");
    AddBookmark("https://site1.com", "Site 1", "OldName");
    AddBookmark("https://site2.com", "Site 2", "OldName");

    storage_->RenameFolder("OldName", "NewName");

    EXPECT_FALSE(storage_->FolderExists("OldName"));
    EXPECT_TRUE(storage_->FolderExists("NewName"));

    auto bookmarks = storage_->GetBookmarksInFolder("NewName");
    EXPECT_EQ(bookmarks.size(), 2u);
}

TEST_F(BookmarkStorageTest, RenameFolder_EmptyNames_NoEffect) {
    CreateFolder("Folder");
    storage_->RenameFolder("", "NewName");
    storage_->RenameFolder("Folder", "");

    EXPECT_TRUE(storage_->FolderExists("Folder"));
}

TEST_F(BookmarkStorageTest, GetNextFolderNumber_NoCollections_Returns1) {
    int next = storage_->GetNextFolderNumber();
    EXPECT_EQ(next, 1);
}

TEST_F(BookmarkStorageTest, GetNextFolderNumber_WithCollections_ReturnsNextNumber) {
    CreateFolder("Collection 1");
    CreateFolder("Collection 3");

    int next = storage_->GetNextFolderNumber();
    EXPECT_EQ(next, 4);
}

TEST_F(BookmarkStorageTest, GetNextFolderNumber_NonCollectionFolders_Ignores) {
    CreateFolder("Work");
    CreateFolder("Personal");

    int next = storage_->GetNextFolderNumber();
    EXPECT_EQ(next, 1);
}

TEST_F(BookmarkStorageTest, GetNextFolderNumber_MixedFolders_OnlyCountsCollections) {
    CreateFolder("Work");
    CreateFolder("Collection 2");
    CreateFolder("Personal");
    CreateFolder("Collection 5");

    int next = storage_->GetNextFolderNumber();
    EXPECT_EQ(next, 6);
}

TEST_F(BookmarkStorageTest, GetNextFolderNumber_ImplicitFolderFromBookmarks) {
    AddBookmark("https://test.google.com", "Example", "Collection 3");

    int next = storage_->GetNextFolderNumber();
    EXPECT_EQ(next, 4);
}

TEST_F(BookmarkStorageTest, EmptyFoldersShowInGetFolders) {
    CreateFolder("EmptyFolder1");
    CreateFolder("EmptyFolder2");
    AddBookmark("https://test.google.com", "Example", "PopulatedFolder");

    auto folders = storage_->GetFolders();
    ASSERT_EQ(folders.size(), 3u);
}
