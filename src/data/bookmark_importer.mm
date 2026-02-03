#include "bookmark_importer.h"
#include "bookmark_storage.h"
#include "utils/filesystem_utils.h"

#import <Foundation/Foundation.h>
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <filesystem>

// Forward declaration
BookmarkStorage* GetBookmarkStorage();

namespace fs = std::filesystem;

std::string BookmarkImporter::GetBrowserName(BrowserType type) {
    switch (type) {
        case BrowserType::Chrome: return "Google Chrome";
        case BrowserType::Safari: return "Safari";
        case BrowserType::Firefox: return "Firefox";
        case BrowserType::Edge: return "Microsoft Edge";
    }
    return "Unknown";
}

std::string BookmarkImporter::GetChromePath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Application Support/Google/Chrome";
}

std::string BookmarkImporter::GetSafariPath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Safari/Bookmarks.plist";
}

std::string BookmarkImporter::GetFirefoxProfilesPath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Application Support/Firefox/Profiles";
}

std::string BookmarkImporter::GetEdgePath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Application Support/Microsoft Edge";
}

std::vector<DetectedBrowser> BookmarkImporter::DetectBrowsers() {
    std::vector<DetectedBrowser> browsers;

    // Check Chrome
    std::string chrome_path = GetChromePath();
    if (!chrome_path.empty() && fs::exists(chrome_path)) {
        // Check for profiles
        std::vector<std::string> profiles = {"Default", "Profile 1", "Profile 2", "Profile 3"};
        for (const auto& profile : profiles) {
            std::string bookmarks_file = chrome_path + "/" + profile + "/Bookmarks";
            if (fs::exists(bookmarks_file)) {
                DetectedBrowser browser;
                browser.type = BrowserType::Chrome;
                browser.name = "Google Chrome";
                browser.profile_path = chrome_path + "/" + profile;
                browser.profile_name = profile;
                browsers.push_back(browser);
            }
        }
    }

    // Check Safari
    std::string safari_path = GetSafariPath();
    if (!safari_path.empty() && fs::exists(safari_path)) {
        DetectedBrowser browser;
        browser.type = BrowserType::Safari;
        browser.name = "Safari";
        browser.profile_path = safari_path;
        browser.profile_name = "Default";
        browsers.push_back(browser);
    }

    // Check Firefox
    std::string firefox_profiles = GetFirefoxProfilesPath();
    if (!firefox_profiles.empty() && fs::exists(firefox_profiles)) {
        for (const auto& entry : fs::directory_iterator(firefox_profiles)) {
            if (entry.is_directory()) {
                std::string places_db = entry.path().string() + "/places.sqlite";
                if (fs::exists(places_db)) {
                    DetectedBrowser browser;
                    browser.type = BrowserType::Firefox;
                    browser.name = "Firefox";
                    browser.profile_path = entry.path().string();
                    browser.profile_name = entry.path().filename().string();
                    browsers.push_back(browser);
                }
            }
        }
    }

    // Check Edge
    std::string edge_path = GetEdgePath();
    if (!edge_path.empty() && fs::exists(edge_path)) {
        std::vector<std::string> profiles = {"Default", "Profile 1", "Profile 2"};
        for (const auto& profile : profiles) {
            std::string bookmarks_file = edge_path + "/" + profile + "/Bookmarks";
            if (fs::exists(bookmarks_file)) {
                DetectedBrowser browser;
                browser.type = BrowserType::Edge;
                browser.name = "Microsoft Edge";
                browser.profile_path = edge_path + "/" + profile;
                browser.profile_name = profile;
                browsers.push_back(browser);
            }
        }
    }

    return browsers;
}

ImportResult BookmarkImporter::Import(const DetectedBrowser& browser, const std::string& folder_name) {
    std::string target_folder = folder_name.empty() ? browser.name : folder_name;

    switch (browser.type) {
        case BrowserType::Chrome:
            return ParseChromeBookmarks(browser.profile_path + "/Bookmarks", target_folder);
        case BrowserType::Safari:
            return ParseSafariBookmarks(browser.profile_path, target_folder);
        case BrowserType::Firefox:
            return ParseFirefoxBookmarks(browser.profile_path + "/places.sqlite", target_folder);
        case BrowserType::Edge:
            return ParseChromeBookmarks(browser.profile_path + "/Bookmarks", target_folder);  // Same format as Chrome
    }

    ImportResult result;
    result.error_message = "Unknown browser type";
    return result;
}

ImportResult BookmarkImporter::ImportFromChrome(const std::string& folder_name) {
    std::string path = GetChromePath() + "/Default/Bookmarks";
    return ParseChromeBookmarks(path, folder_name.empty() ? "Google Chrome" : folder_name);
}

ImportResult BookmarkImporter::ImportFromSafari(const std::string& folder_name) {
    return ParseSafariBookmarks(GetSafariPath(), folder_name.empty() ? "Safari" : folder_name);
}

ImportResult BookmarkImporter::ImportFromFirefox(const std::string& folder_name) {
    // Find default Firefox profile
    std::string profiles_path = GetFirefoxProfilesPath();
    if (profiles_path.empty() || !fs::exists(profiles_path)) {
        ImportResult result;
        result.error_message = "Firefox profiles directory not found";
        return result;
    }

    // Find first profile with places.sqlite
    for (const auto& entry : fs::directory_iterator(profiles_path)) {
        if (entry.is_directory()) {
            std::string places_db = entry.path().string() + "/places.sqlite";
            if (fs::exists(places_db)) {
                return ParseFirefoxBookmarks(places_db, folder_name.empty() ? "Firefox" : folder_name);
            }
        }
    }

    ImportResult result;
    result.error_message = "No Firefox profile with bookmarks found";
    return result;
}

ImportResult BookmarkImporter::ImportFromEdge(const std::string& folder_name) {
    std::string path = GetEdgePath() + "/Default/Bookmarks";
    return ParseChromeBookmarks(path, folder_name.empty() ? "Microsoft Edge" : folder_name);
}

// Helper to recursively parse Chrome/Edge bookmark JSON
static void ParseChromeBookmarkNode(const std::string& json, size_t& pos,
                                     const std::string& folder_name,
                                     int& imported, int& skipped) {
    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) return;

    // Simple JSON parser for Chrome bookmarks format
    // Looking for: "type": "url", "name": "...", "url": "..."
    while (pos < json.length()) {
        size_t type_pos = json.find("\"type\"", pos);
        if (type_pos == std::string::npos) break;

        size_t type_value_start = json.find("\"", type_pos + 7);
        if (type_value_start == std::string::npos) break;
        type_value_start++;

        size_t type_value_end = json.find("\"", type_value_start);
        if (type_value_end == std::string::npos) break;

        std::string type = json.substr(type_value_start, type_value_end - type_value_start);
        pos = type_value_end + 1;

        if (type == "url") {
            // Find name and url
            size_t block_end = json.find("}", pos);
            if (block_end == std::string::npos) break;

            std::string block = json.substr(pos, block_end - pos);

            // Extract name
            std::string name;
            size_t name_pos = block.find("\"name\"");
            if (name_pos != std::string::npos) {
                size_t name_start = block.find("\"", name_pos + 7);
                if (name_start != std::string::npos) {
                    name_start++;
                    size_t name_end = block.find("\"", name_start);
                    if (name_end != std::string::npos) {
                        name = block.substr(name_start, name_end - name_start);
                    }
                }
            }

            // Extract URL
            std::string url;
            size_t url_pos = block.find("\"url\"");
            if (url_pos != std::string::npos) {
                size_t url_start = block.find("\"", url_pos + 6);
                if (url_start != std::string::npos) {
                    url_start++;
                    size_t url_end = block.find("\"", url_start);
                    if (url_end != std::string::npos) {
                        url = block.substr(url_start, url_end - url_start);
                    }
                }
            }

            if (!url.empty() && !name.empty()) {
                // Check for duplicate
                if (storage->IsBookmarked(url)) {
                    skipped++;
                } else {
                    storage->AddBookmark(url, name, folder_name);
                    imported++;
                }
            }

            pos = block_end + 1;
        } else if (type == "folder") {
            // Skip folder structure, import all bookmarks flat into target folder
            pos = type_value_end + 1;
        }
    }
}

ImportResult BookmarkImporter::ParseChromeBookmarks(const std::string& path, const std::string& folder_name) {
    ImportResult result;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.error_message = "Could not open Chrome bookmarks file";
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) {
        result.error_message = "Bookmark storage not initialized";
        return result;
    }

    // Create folder if it doesn't exist
    if (!folder_name.empty() && !storage->FolderExists(folder_name)) {
        storage->CreateFolder(folder_name);
    }

    size_t pos = 0;
    ParseChromeBookmarkNode(json, pos, folder_name, result.imported_count, result.skipped_count);

    result.success = true;
    return result;
}

ImportResult BookmarkImporter::ParseSafariBookmarks(const std::string& path, const std::string& folder_name) {
    ImportResult result;

    @autoreleasepool {
        NSString* plistPath = [NSString stringWithUTF8String:path.c_str()];
        NSDictionary* plist = [NSDictionary dictionaryWithContentsOfFile:plistPath];

        if (!plist) {
            result.error_message = "Could not read Safari bookmarks plist";
            return result;
        }

        BookmarkStorage* storage = GetBookmarkStorage();
        if (!storage) {
            result.error_message = "Bookmark storage not initialized";
            return result;
        }

        // Create folder if needed
        if (!folder_name.empty() && !storage->FolderExists(folder_name)) {
            storage->CreateFolder(folder_name);
        }

        // Safari bookmarks are nested under "Children" arrays
        // Recursively process the structure
        std::function<void(NSArray*)> processChildren = [&](NSArray* children) {
            if (!children) return;

            for (NSDictionary* item in children) {
                NSString* type = item[@"WebBookmarkType"];

                if ([type isEqualToString:@"WebBookmarkTypeLeaf"]) {
                    // This is a bookmark
                    NSString* urlString = item[@"URLString"];
                    NSDictionary* uriDict = item[@"URIDictionary"];
                    NSString* title = uriDict[@"title"];

                    if (!title) {
                        title = item[@"Title"];
                    }

                    if (urlString && title) {
                        std::string url = [urlString UTF8String];
                        std::string name = [title UTF8String];

                        if (storage->IsBookmarked(url)) {
                            result.skipped_count++;
                        } else {
                            storage->AddBookmark(url, name, folder_name);
                            result.imported_count++;
                        }
                    }
                } else if ([type isEqualToString:@"WebBookmarkTypeList"]) {
                    // This is a folder, recurse into its children
                    NSArray* subChildren = item[@"Children"];
                    processChildren(subChildren);
                }
            }
        };

        NSArray* rootChildren = plist[@"Children"];
        processChildren(rootChildren);

        result.success = true;
    }

    return result;
}

ImportResult BookmarkImporter::ParseFirefoxBookmarks(const std::string& path, const std::string& folder_name) {
    ImportResult result;

    // Firefox stores bookmarks in places.sqlite
    // We need to copy it first because Firefox may have it locked
    std::string temp_db = orbfox::utils::GetAppSupportPath() + "/firefox_import_temp.db";

    try {
        fs::copy_file(path, temp_db, fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        result.error_message = "Could not copy Firefox database: " + std::string(e.what());
        return result;
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(temp_db.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        result.error_message = "Could not open Firefox database";
        fs::remove(temp_db);
        return result;
    }

    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) {
        sqlite3_close(db);
        fs::remove(temp_db);
        result.error_message = "Bookmark storage not initialized";
        return result;
    }

    // Create folder if needed
    if (!folder_name.empty() && !storage->FolderExists(folder_name)) {
        storage->CreateFolder(folder_name);
    }

    // Query bookmarks from Firefox places database
    // moz_bookmarks has fk (foreign key to moz_places), title
    // moz_places has url
    const char* sql = R"(
        SELECT p.url, b.title
        FROM moz_bookmarks b
        JOIN moz_places p ON b.fk = p.id
        WHERE b.type = 1 AND p.url NOT LIKE 'place:%'
    )";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        fs::remove(temp_db);
        result.error_message = "Could not query Firefox bookmarks";
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        if (url && title) {
            std::string url_str(url);
            std::string title_str(title);

            if (storage->IsBookmarked(url_str)) {
                result.skipped_count++;
            } else {
                storage->AddBookmark(url_str, title_str, folder_name);
                result.imported_count++;
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    fs::remove(temp_db);

    result.success = true;
    return result;
}
