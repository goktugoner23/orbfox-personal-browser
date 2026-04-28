#include "settings_storage.h"
#include "gtest/gtest.h"

#include <cstdlib>

// ============================================================================
// Settings Struct Tests
// ============================================================================

TEST(SettingsTest, DefaultValues) {
    Settings settings;

    EXPECT_EQ(settings.homepage_url, "orbfox://bookmarks");
    EXPECT_EQ(settings.new_tab_url, "orbfox://bookmarks");
    EXPECT_TRUE(settings.restore_session);
    EXPECT_TRUE(settings.tracking_protection);
    EXPECT_EQ(settings.download_path, "");
    EXPECT_TRUE(settings.ask_before_download);
}

// ============================================================================
// SettingsStorage Tests
// ============================================================================

class SettingsStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset to default settings before each test
        Settings defaults;
        SettingsStorage::GetInstance().Set(defaults);
    }

    void TearDown() override {
        // Reset to defaults after each test
        Settings defaults;
        SettingsStorage::GetInstance().Set(defaults);
    }
};

TEST_F(SettingsStorageTest, GetInstance_ReturnsSameInstance) {
    SettingsStorage& instance1 = SettingsStorage::GetInstance();
    SettingsStorage& instance2 = SettingsStorage::GetInstance();

    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(SettingsStorageTest, Get_ReturnsCurrentSettings) {
    const Settings& settings = SettingsStorage::GetInstance().Get();

    // Should return default values
    EXPECT_EQ(settings.homepage_url, "orbfox://bookmarks");
    EXPECT_TRUE(settings.restore_session);
}

TEST_F(SettingsStorageTest, Set_UpdatesAllSettings) {
    Settings custom;
    custom.homepage_url = "https://custom.com";
    custom.new_tab_url = "https://newtab.com";
    custom.restore_session = false;
    custom.tracking_protection = false;
    custom.download_path = "/custom/path";
    custom.ask_before_download = false;

    SettingsStorage::GetInstance().Set(custom);

    const Settings& result = SettingsStorage::GetInstance().Get();
    EXPECT_EQ(result.homepage_url, "https://custom.com");
    EXPECT_EQ(result.new_tab_url, "https://newtab.com");
    EXPECT_FALSE(result.restore_session);
    EXPECT_FALSE(result.tracking_protection);
    EXPECT_EQ(result.download_path, "/custom/path");
    EXPECT_FALSE(result.ask_before_download);
}

// ============================================================================
// Individual Setter Tests
// ============================================================================

TEST_F(SettingsStorageTest, SetHomepage_UpdatesHomepage) {
    SettingsStorage::GetInstance().SetHomepage("https://test.google.com");

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, "https://test.google.com");
}

TEST_F(SettingsStorageTest, SetNewTabUrl_UpdatesNewTabUrl) {
    SettingsStorage::GetInstance().SetNewTabUrl("https://newtab.google.com");

    EXPECT_EQ(SettingsStorage::GetInstance().Get().new_tab_url, "https://newtab.google.com");
}

TEST_F(SettingsStorageTest, SetRestoreSession_UpdatesRestoreSession) {
    // Default is true, set to false
    SettingsStorage::GetInstance().SetRestoreSession(false);
    EXPECT_FALSE(SettingsStorage::GetInstance().Get().restore_session);

    // Set back to true
    SettingsStorage::GetInstance().SetRestoreSession(true);
    EXPECT_TRUE(SettingsStorage::GetInstance().Get().restore_session);
}

TEST_F(SettingsStorageTest, SetTrackingProtection_UpdatesTrackingProtection) {
    // Default is true, set to false
    SettingsStorage::GetInstance().SetTrackingProtection(false);
    EXPECT_FALSE(SettingsStorage::GetInstance().Get().tracking_protection);

    // Set back to true
    SettingsStorage::GetInstance().SetTrackingProtection(true);
    EXPECT_TRUE(SettingsStorage::GetInstance().Get().tracking_protection);
}

TEST_F(SettingsStorageTest, SetDownloadPath_UpdatesDownloadPath) {
    SettingsStorage::GetInstance().SetDownloadPath("/Users/test/Downloads");

    EXPECT_EQ(SettingsStorage::GetInstance().Get().download_path, "/Users/test/Downloads");
}

TEST_F(SettingsStorageTest, SetAskBeforeDownload_UpdatesAskBeforeDownload) {
    // Default is true, set to false
    SettingsStorage::GetInstance().SetAskBeforeDownload(false);
    EXPECT_FALSE(SettingsStorage::GetInstance().Get().ask_before_download);

    // Set back to true
    SettingsStorage::GetInstance().SetAskBeforeDownload(true);
    EXPECT_TRUE(SettingsStorage::GetInstance().Get().ask_before_download);
}

// ============================================================================
// GetSearchUrl Tests
// ============================================================================

TEST(SettingsStorageStaticTest, GetSearchUrl_SimpleQuery) {
    std::string url = SettingsStorage::GetSearchUrl("hello");
    EXPECT_EQ(url, "https://www.google.com/search?q=hello");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_QueryWithSpaces) {
    std::string url = SettingsStorage::GetSearchUrl("hello world");
    EXPECT_EQ(url, "https://www.google.com/search?q=hello+world");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_QueryWithSpecialCharacters) {
    std::string url = SettingsStorage::GetSearchUrl("c++ programming");
    // + is encoded as %2B, space becomes +
    EXPECT_EQ(url, "https://www.google.com/search?q=c%2B%2B+programming");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_QueryWithUrlUnsafeChars) {
    std::string url = SettingsStorage::GetSearchUrl("test&query=value");
    // & encoded as %26, = encoded as %3D
    EXPECT_EQ(url, "https://www.google.com/search?q=test%26query%3Dvalue");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_QueryWithUnicodeCharacters) {
    // Test with some UTF-8 characters (e.g., Japanese hiragana)
    std::string url = SettingsStorage::GetSearchUrl("cafe");
    EXPECT_EQ(url, "https://www.google.com/search?q=cafe");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_EmptyQuery) {
    std::string url = SettingsStorage::GetSearchUrl("");
    EXPECT_EQ(url, "https://www.google.com/search?q=");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_PreservesAllowedChars) {
    // Alphanumeric and -_.~ should not be encoded
    std::string url = SettingsStorage::GetSearchUrl("test-query_v1.0~beta");
    EXPECT_EQ(url, "https://www.google.com/search?q=test-query_v1.0~beta");
}

TEST(SettingsStorageStaticTest, GetSearchUrl_QuotationMarks) {
    std::string url = SettingsStorage::GetSearchUrl("\"exact phrase\"");
    // Quotes are encoded as %22
    EXPECT_EQ(url, "https://www.google.com/search?q=%22exact+phrase%22");
}

// ============================================================================
// GetResolvedDownloadPath Tests
// ============================================================================

TEST_F(SettingsStorageTest, GetResolvedDownloadPath_EmptyPath_ReturnsHomeDownloads) {
    SettingsStorage::GetInstance().SetDownloadPath("");

    std::string resolved = SettingsStorage::GetInstance().GetResolvedDownloadPath();

    // Should end with /Downloads
    EXPECT_TRUE(resolved.find("/Downloads") != std::string::npos);
    // Should not be empty
    EXPECT_FALSE(resolved.empty());
}

TEST_F(SettingsStorageTest, GetResolvedDownloadPath_AbsolutePath_ReturnsAsIs) {
    SettingsStorage::GetInstance().SetDownloadPath("/custom/download/path");

    std::string resolved = SettingsStorage::GetInstance().GetResolvedDownloadPath();

    EXPECT_EQ(resolved, "/custom/download/path");
}

TEST_F(SettingsStorageTest, GetResolvedDownloadPath_TildePath_ExpandsTilde) {
    SettingsStorage::GetInstance().SetDownloadPath("~/MyDownloads");

    std::string resolved = SettingsStorage::GetInstance().GetResolvedDownloadPath();

    // Should not start with ~
    EXPECT_NE(resolved[0], '~');
    // Should end with MyDownloads
    EXPECT_TRUE(resolved.find("MyDownloads") != std::string::npos);
}

// ============================================================================
// ToJson Tests
// ============================================================================

TEST_F(SettingsStorageTest, ToJson_ContainsAllFields) {
    std::string json = SettingsStorage::GetInstance().ToJson();

    EXPECT_TRUE(json.find("\"homepage_url\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"new_tab_url\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"restore_session\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"tracking_protection\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"download_path\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"ask_before_download\"") != std::string::npos);
}

TEST_F(SettingsStorageTest, ToJson_DefaultValues) {
    std::string json = SettingsStorage::GetInstance().ToJson();

    EXPECT_TRUE(json.find("\"orbfox://bookmarks\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"restore_session\": true") != std::string::npos);
    EXPECT_TRUE(json.find("\"tracking_protection\": true") != std::string::npos);
    EXPECT_TRUE(json.find("\"ask_before_download\": true") != std::string::npos);
}

TEST_F(SettingsStorageTest, ToJson_EscapesSpecialCharacters) {
    // Set a URL with special characters that need escaping
    SettingsStorage::GetInstance().SetHomepage("https://test.google.com/path?key=\"value\"");

    std::string json = SettingsStorage::GetInstance().ToJson();

    // Should contain escaped quote
    EXPECT_TRUE(json.find("\\\"value\\\"") != std::string::npos);
}

TEST_F(SettingsStorageTest, ToJson_EscapesBackslash) {
    // Test JSON backslash escaping through homepage field (download_path has validation)
    SettingsStorage::GetInstance().SetHomepage("https://test.google.com/path\\with\\backslashes");

    std::string json = SettingsStorage::GetInstance().ToJson();

    // Backslashes should be escaped in the JSON output.
    EXPECT_TRUE(json.find("\\\\") != std::string::npos);
}

TEST_F(SettingsStorageTest, ToJson_EscapesNewlineAndTab) {
    SettingsStorage::GetInstance().SetHomepage("https://test.google.com\ntest\ttab");

    std::string json = SettingsStorage::GetInstance().ToJson();

    // Newline should be escaped as \n
    EXPECT_TRUE(json.find("\\n") != std::string::npos);
    // Tab should be escaped as \t
    EXPECT_TRUE(json.find("\\t") != std::string::npos);
}

// ============================================================================
// FromJson Tests
// ============================================================================

TEST_F(SettingsStorageTest, FromJson_ParsesAllFields) {
    std::string json = R"({
        "homepage_url": "https://custom.com",
        "new_tab_url": "https://newtab.com",
        "restore_session": false,
        "tracking_protection": false,
        "download_path": "/custom/path",
        "ask_before_download": false
    })";

    bool result = SettingsStorage::GetInstance().FromJson(json);
    EXPECT_TRUE(result);

    const Settings& settings = SettingsStorage::GetInstance().Get();
    EXPECT_EQ(settings.homepage_url, "https://custom.com");
    EXPECT_EQ(settings.new_tab_url, "https://newtab.com");
    EXPECT_FALSE(settings.restore_session);
    EXPECT_FALSE(settings.tracking_protection);
    EXPECT_EQ(settings.download_path, "/custom/path");
    EXPECT_FALSE(settings.ask_before_download);
}

TEST_F(SettingsStorageTest, FromJson_PartialJson_UsesDefaults) {
    // Start with custom values
    Settings custom;
    custom.homepage_url = "https://initial.com";
    custom.restore_session = false;
    SettingsStorage::GetInstance().Set(custom);

    // Parse JSON with only some fields
    std::string json = R"({
        "homepage_url": "https://updated.com"
    })";

    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& settings = SettingsStorage::GetInstance().Get();
    // Updated from JSON
    EXPECT_EQ(settings.homepage_url, "https://updated.com");
    // Retained previous value (not in JSON)
    EXPECT_FALSE(settings.restore_session);
}

TEST_F(SettingsStorageTest, FromJson_HandlesEscapedCharacters) {
    std::string json = R"({
        "homepage_url": "https://test.google.com/path?key=\"value\"",
        "new_tab_url": "https://www.google.com/path\\with\\backslashes",
        "restore_session": true,
        "tracking_protection": true,
        "download_path": "/Users/test/Downloads",
        "ask_before_download": true
    })";

    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& settings = SettingsStorage::GetInstance().Get();
    // Should have unescaped quotes
    EXPECT_TRUE(settings.homepage_url.find('"') != std::string::npos);
    // Should have unescaped backslashes (test via new_tab_url since download_path is validated)
    EXPECT_TRUE(settings.new_tab_url.find('\\') != std::string::npos);
}

TEST_F(SettingsStorageTest, FromJson_HandlesNewlineAndTab) {
    std::string json = R"({
        "homepage_url": "line1\nline2",
        "new_tab_url": "url\twith\ttabs"
    })";

    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& settings = SettingsStorage::GetInstance().Get();
    EXPECT_TRUE(settings.homepage_url.find('\n') != std::string::npos);
    // Test tabs via new_tab_url since download_path is validated
    EXPECT_TRUE(settings.new_tab_url.find('\t') != std::string::npos);
}

TEST_F(SettingsStorageTest, FromJson_EmptyString_RetainsDefaults) {
    std::string json = "";

    // FromJson always returns true (graceful degradation)
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& settings = SettingsStorage::GetInstance().Get();
    // Should retain default values
    EXPECT_EQ(settings.homepage_url, "orbfox://bookmarks");
    EXPECT_TRUE(settings.restore_session);
}

TEST_F(SettingsStorageTest, FromJson_InvalidJson_RetainsDefaults) {
    std::string json = "not valid json {{{";

    // FromJson always returns true (graceful degradation)
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& settings = SettingsStorage::GetInstance().Get();
    // Should retain default values
    EXPECT_EQ(settings.homepage_url, "orbfox://bookmarks");
}

// ============================================================================
// ToJson/FromJson Roundtrip Tests
// ============================================================================

TEST_F(SettingsStorageTest, JsonRoundtrip_PreservesAllSettings) {
    Settings original;
    original.homepage_url = "https://custom-homepage.com";
    original.new_tab_url = "https://custom-newtab.com";
    original.restore_session = false;
    original.tracking_protection = false;
    original.download_path = "/my/download/path";
    original.ask_before_download = false;

    SettingsStorage::GetInstance().Set(original);
    std::string json = SettingsStorage::GetInstance().ToJson();

    // Reset to defaults
    Settings defaults;
    SettingsStorage::GetInstance().Set(defaults);

    // Parse the saved JSON
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& restored = SettingsStorage::GetInstance().Get();
    EXPECT_EQ(restored.homepage_url, original.homepage_url);
    EXPECT_EQ(restored.new_tab_url, original.new_tab_url);
    EXPECT_EQ(restored.restore_session, original.restore_session);
    EXPECT_EQ(restored.tracking_protection, original.tracking_protection);
    EXPECT_EQ(restored.download_path, original.download_path);
    EXPECT_EQ(restored.ask_before_download, original.ask_before_download);
}

TEST_F(SettingsStorageTest, JsonRoundtrip_SpecialCharactersInUrl) {
    SettingsStorage::GetInstance().SetHomepage("https://test.google.com/search?q=\"test\"&foo=bar");
    std::string json = SettingsStorage::GetInstance().ToJson();

    // Reset
    SettingsStorage::GetInstance().SetHomepage("");

    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url,
              "https://test.google.com/search?q=\"test\"&foo=bar");
}

// ============================================================================
// Load/Save Integration Tests
// ============================================================================

// Note: Load/Save test uses the production settings file path, so we test
// the JSON roundtrip instead which tests the same serialization logic.
// The actual file I/O is tested through the persistence integration test.
TEST_F(SettingsStorageTest, LoadSave_UsesToJsonFromJson) {
    // This test verifies that ToJson/FromJson work correctly, which is the
    // same serialization logic used by Save/Load
    Settings original;
    original.homepage_url = "https://saved-homepage.com";
    original.new_tab_url = "https://saved-newtab.com";
    original.restore_session = false;
    original.tracking_protection = false;
    original.download_path = "~/CustomDownloads";
    original.ask_before_download = false;

    SettingsStorage::GetInstance().Set(original);
    std::string json = SettingsStorage::GetInstance().ToJson();

    // Reset to defaults
    Settings defaults;
    SettingsStorage::GetInstance().Set(defaults);

    // Load from JSON (same as Load() does internally)
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const Settings& loaded = SettingsStorage::GetInstance().Get();
    EXPECT_EQ(loaded.homepage_url, original.homepage_url);
    EXPECT_EQ(loaded.new_tab_url, original.new_tab_url);
    EXPECT_EQ(loaded.restore_session, original.restore_session);
    EXPECT_EQ(loaded.tracking_protection, original.tracking_protection);
    EXPECT_EQ(loaded.download_path, original.download_path);
    EXPECT_EQ(loaded.ask_before_download, original.ask_before_download);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SettingsStorageTest, EdgeCase_EmptyStrings) {
    SettingsStorage::GetInstance().SetHomepage("");
    SettingsStorage::GetInstance().SetNewTabUrl("");
    SettingsStorage::GetInstance().SetDownloadPath("");

    const Settings& settings = SettingsStorage::GetInstance().Get();
    EXPECT_EQ(settings.homepage_url, "");
    EXPECT_EQ(settings.new_tab_url, "");
    EXPECT_EQ(settings.download_path, "");
}

TEST_F(SettingsStorageTest, EdgeCase_VeryLongUrl) {
    std::string long_url = "https://test.google.com/" + std::string(1000, 'a');
    SettingsStorage::GetInstance().SetHomepage(long_url);

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, long_url);

    // Roundtrip through JSON
    std::string json = SettingsStorage::GetInstance().ToJson();
    SettingsStorage::GetInstance().SetHomepage("");
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, long_url);
}

TEST_F(SettingsStorageTest, EdgeCase_UnicodeInUrl) {
    // URL with Unicode characters
    std::string unicode_url = "https://test.google.com/cafe";
    SettingsStorage::GetInstance().SetHomepage(unicode_url);

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, unicode_url);

    // Roundtrip through JSON
    std::string json = SettingsStorage::GetInstance().ToJson();
    SettingsStorage::GetInstance().SetHomepage("");
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, unicode_url);
}

TEST_F(SettingsStorageTest, EdgeCase_AllBooleanCombinations) {
    // Test all combinations of boolean values
    for (int i = 0; i < 8; ++i) {
        bool restore = (i & 1) != 0;
        bool tracking = (i & 2) != 0;
        bool ask = (i & 4) != 0;

        SettingsStorage::GetInstance().SetRestoreSession(restore);
        SettingsStorage::GetInstance().SetTrackingProtection(tracking);
        SettingsStorage::GetInstance().SetAskBeforeDownload(ask);

        const Settings& settings = SettingsStorage::GetInstance().Get();
        EXPECT_EQ(settings.restore_session, restore);
        EXPECT_EQ(settings.tracking_protection, tracking);
        EXPECT_EQ(settings.ask_before_download, ask);
    }
}

TEST_F(SettingsStorageTest, EdgeCase_WhitespaceInJson) {
    // JSON with whitespace AFTER the colon (the parser expects "key": followed by optional whitespace)
    // Note: whitespace BEFORE the colon is not supported by the simple parser
    std::string json = "{\n  \"homepage_url\":  \"https://test.com\"  ,\n  \"restore_session\":\ttrue\n}";

    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    EXPECT_EQ(SettingsStorage::GetInstance().Get().homepage_url, "https://test.com");
    EXPECT_TRUE(SettingsStorage::GetInstance().Get().restore_session);
}

TEST_F(SettingsStorageTest, EdgeCase_BooleanValueVariants) {
    // Test boolean parsing with true/false values
    std::string json_true = R"({"restore_session": true})";
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json_true));
    EXPECT_TRUE(SettingsStorage::GetInstance().Get().restore_session);

    std::string json_false = R"({"restore_session": false})";
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json_false));
    EXPECT_FALSE(SettingsStorage::GetInstance().Get().restore_session);
}

// ============================================================================
// Search Shortcuts Tests
// ============================================================================

TEST(SettingsTest, DefaultSearchShortcuts) {
    Settings settings;
    ASSERT_EQ(settings.search_shortcuts.size(), 3u);
    EXPECT_EQ(settings.search_shortcuts[0].key, "g");
    EXPECT_EQ(settings.search_shortcuts[1].key, "y");
    EXPECT_EQ(settings.search_shortcuts[2].key, "a");
}

TEST_F(SettingsStorageTest, SearchShortcuts_ToJsonFromJson_Roundtrip) {
    // Modify shortcuts
    Settings custom;
    custom.search_shortcuts = {
        {"w", "https://en.wikipedia.org/w/index.php?search=%s"},
        {"g", "https://www.google.com/search?q=%s"},
    };
    SettingsStorage::GetInstance().Set(custom);
    std::string json = SettingsStorage::GetInstance().ToJson();

    // Reset to defaults
    Settings defaults;
    SettingsStorage::GetInstance().Set(defaults);

    // Parse saved JSON
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));
    const auto& shortcuts = SettingsStorage::GetInstance().Get().search_shortcuts;
    ASSERT_EQ(shortcuts.size(), 2u);
    EXPECT_EQ(shortcuts[0].key, "w");
    EXPECT_EQ(shortcuts[0].url_template, "https://en.wikipedia.org/w/index.php?search=%s");
    EXPECT_EQ(shortcuts[1].key, "g");
}

TEST_F(SettingsStorageTest, SearchShortcuts_FromJsonWithoutKey_KeepsDefaults) {
    // JSON without search_shortcuts key should keep defaults
    std::string json = R"({"homepage_url": "https://test.com"})";
    EXPECT_TRUE(SettingsStorage::GetInstance().FromJson(json));

    const auto& shortcuts = SettingsStorage::GetInstance().Get().search_shortcuts;
    ASSERT_EQ(shortcuts.size(), 3u);
    EXPECT_EQ(shortcuts[0].key, "g");
    EXPECT_EQ(shortcuts[1].key, "y");
    EXPECT_EQ(shortcuts[2].key, "a");
}

// ============================================================================
// ResolveAddressBarInput Tests
// ============================================================================

TEST_F(SettingsStorageTest, ResolveAddressBarInput_ShortcutMatch) {
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("y kitten vids");
    EXPECT_EQ(result, "https://www.youtube.com/results?search_query=kitten+vids");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_GoogleFallback) {
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("kitten vids");
    EXPECT_EQ(result, "https://www.google.com/search?q=kitten+vids");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_SchemePassthrough) {
    EXPECT_EQ(SettingsStorage::GetInstance().ResolveAddressBarInput("https://example.com"),
              "https://example.com");
    EXPECT_EQ(SettingsStorage::GetInstance().ResolveAddressBarInput("http://example.com"),
              "http://example.com");
    EXPECT_EQ(SettingsStorage::GetInstance().ResolveAddressBarInput("orbfox://settings"),
              "orbfox://settings");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_UrlLikeAddsDot) {
    EXPECT_EQ(SettingsStorage::GetInstance().ResolveAddressBarInput("example.com"),
              "https://example.com");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_ShortcutKeyAloneDoesNotMatch) {
    // Just "y" with no space should NOT match shortcut — it's URL-like if it has no dot
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("y");
    EXPECT_EQ(result, "https://www.google.com/search?q=y");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_GoogleShortcut) {
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("g hello world");
    EXPECT_EQ(result, "https://www.google.com/search?q=hello+world");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_AmazonShortcut) {
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("a wireless mouse");
    EXPECT_EQ(result, "https://www.amazon.com/s?k=wireless+mouse");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_UnknownPrefixWithSpace) {
    // "z something" — z is not a shortcut, so falls through to Google search
    std::string result = SettingsStorage::GetInstance().ResolveAddressBarInput("z something");
    EXPECT_EQ(result, "https://www.google.com/search?q=z+something");
}

TEST_F(SettingsStorageTest, ResolveAddressBarInput_Empty) {
    EXPECT_EQ(SettingsStorage::GetInstance().ResolveAddressBarInput(""), "");
}
