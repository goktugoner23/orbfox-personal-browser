// Cloud sync service implementation using Firebase Realtime Database

#import <Foundation/Foundation.h>

#include "sync/sync_service.h"
#include "sync/google_auth.h"
#include "bookmark_storage.h"
#include "history_storage.h"
#include "settings_storage.h"
#include "session_storage.h"
#include "utils/json_utils.h"

#include <functional>
#include <sstream>
#include <thread>

namespace {

std::string RunStringOnMainThread(const std::function<std::string()>& work) {
    if ([NSThread isMainThread]) {
        return work();
    }

    __block std::string result;
    dispatch_sync(dispatch_get_main_queue(), ^{
        result = work();
    });
    return result;
}

bool RunBoolOnMainThread(const std::function<bool()>& work) {
    if ([NSThread isMainThread]) {
        return work();
    }

    __block bool result = false;
    dispatch_sync(dispatch_get_main_queue(), ^{
        result = work();
    });
    return result;
}

// Firebase RTDB REST accepts an ID token only via the ?auth= query param,
// not the Authorization: Bearer header (that's for OAuth2 access tokens).
std::string WithAuth(const std::string& url, const std::string& token) {
    if (token.empty()) {
        return url;
    }
    char sep = url.find('?') == std::string::npos ? '?' : '&';
    return url + sep + "auth=" + token;
}

}  // namespace

SyncService& SyncService::GetInstance() {
    static SyncService instance;
    return instance;
}

// JSON export helpers
std::string SyncService::ExportBookmarksToJson() {
    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage) return "[]";

    auto bookmarks = storage->GetAllBookmarks();
    auto folders = storage->GetFolders();

    std::ostringstream ss;
    ss << "{\"bookmarks\":[";

    bool first = true;
    for (const auto& bm : bookmarks) {
        if (!first) ss << ",";
        first = false;
        ss << "{";
        ss << "\"url\":\"" << orbfox::utils::EscapeJsonString(bm.url) << "\",";
        ss << "\"title\":\"" << orbfox::utils::EscapeJsonString(bm.title) << "\",";
        ss << "\"folder\":\"" << orbfox::utils::EscapeJsonString(bm.folder) << "\",";
        ss << "\"created_time\":" << bm.created_time << ",";
        ss << "\"position\":" << bm.position;
        ss << "}";
    }

    ss << "],\"folders\":[";

    first = true;
    for (const auto& folder : folders) {
        if (!first) ss << ",";
        first = false;
        int pos = storage->GetFolderPosition(folder);
        ss << "{";
        ss << "\"name\":\"" << orbfox::utils::EscapeJsonString(folder) << "\",";
        ss << "\"position\":" << pos;
        ss << "}";
    }

    ss << "]}";
    return ss.str();
}

std::string SyncService::ExportHistoryToJson() {
    HistoryStorage* storage = GetHistoryStorage();
    if (!storage) return "[]";

    // Export last 1000 history entries
    auto history = storage->GetRecentHistory(1000);

    std::ostringstream ss;
    ss << "[";

    bool first = true;
    for (const auto& entry : history) {
        if (!first) ss << ",";
        first = false;
        ss << "{";
        ss << "\"url\":\"" << orbfox::utils::EscapeJsonString(entry.url) << "\",";
        ss << "\"title\":\"" << orbfox::utils::EscapeJsonString(entry.title) << "\",";
        ss << "\"visit_time\":" << entry.visit_time << ",";
        ss << "\"visit_count\":" << entry.visit_count;
        ss << "}";
    }

    ss << "]";
    return ss.str();
}

std::string SyncService::ExportSettingsToJson() {
    return SettingsStorage::GetInstance().ToJson();
}

std::string SyncService::ExportSessionToJson() {
    return SessionStorage().ReadRawJson();
}

// JSON import helpers
bool SyncService::ImportBookmarksFromJson(const std::string& json) {
    using namespace orbfox::utils;

    BookmarkStorage* storage = GetBookmarkStorage();
    if (!storage || json.empty()) return false;

    // Parse bookmarks array
    size_t bookmarks_start = json.find("\"bookmarks\":[");
    if (bookmarks_start == std::string::npos) return false;

    bookmarks_start = json.find('[', bookmarks_start);
    size_t bookmarks_end = json.find(']', bookmarks_start);
    if (bookmarks_end == std::string::npos) return false;

    std::string bookmarks_array = json.substr(bookmarks_start, bookmarks_end - bookmarks_start + 1);

    // Simple parsing - find each object
    size_t pos = 0;
    while ((pos = bookmarks_array.find('{', pos)) != std::string::npos) {
        size_t end = bookmarks_array.find('}', pos);
        if (end == std::string::npos) break;

        std::string obj = bookmarks_array.substr(pos, end - pos + 1);

        std::string url = GetJsonString(obj, "url", "");
        std::string title = GetJsonString(obj, "title", "");
        std::string folder = GetJsonString(obj, "folder", "");

        if (!url.empty() && !storage->IsBookmarked(url)) {
            if (storage->AddBookmark(url, title, folder) <= 0) {
                return false;
            }
        }

        pos = end + 1;
    }

    // Parse and create folders
    size_t folders_start = json.find("\"folders\":[");
    if (folders_start != std::string::npos) {
        folders_start = json.find('[', folders_start);
        size_t folders_end = json.find(']', folders_start);
        if (folders_end != std::string::npos) {
            std::string folders_array = json.substr(folders_start, folders_end - folders_start + 1);

            pos = 0;
            while ((pos = folders_array.find('{', pos)) != std::string::npos) {
                size_t end = folders_array.find('}', pos);
                if (end == std::string::npos) break;

                std::string obj = folders_array.substr(pos, end - pos + 1);
                std::string name = GetJsonString(obj, "name", "");

                if (!name.empty() && !storage->FolderExists(name)) {
                    if (!storage->CreateFolder(name)) {
                        return false;
                    }
                }

                pos = end + 1;
            }
        }
    }

    return true;
}

bool SyncService::ImportHistoryFromJson(const std::string& json) {
    using namespace orbfox::utils;

    HistoryStorage* storage = GetHistoryStorage();
    if (!storage || json.empty() || json == "[]") return false;

    // Simple parsing - find each object
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;

        std::string obj = json.substr(pos, end - pos + 1);

        std::string url = GetJsonString(obj, "url", "");
        std::string title = GetJsonString(obj, "title", "");

        if (!url.empty()) {
            storage->AddEntry(url, title);
        }

        pos = end + 1;
    }

    return true;
}

bool SyncService::ImportSettingsFromJson(const std::string& json) {
    if (json.empty()) return false;
    return SettingsStorage::GetInstance().FromJson(json);
}

bool SyncService::ImportSessionFromJson(const std::string& json) {
    // Written to disk; applied on next launch (live tabs are not torn down).
    return SessionStorage().WriteRawJson(json);
}

// HTTP helpers using NSURLSession
std::string SyncService::HttpPut(const std::string& url, const std::string& body, const std::string& token) {
    @autoreleasepool {
        std::string authUrl = WithAuth(url, token);
        NSURL* nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:authUrl.c_str()]];
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:nsUrl];
        request.HTTPMethod = @"PUT";
        request.HTTPBody = [NSData dataWithBytes:body.c_str() length:body.size()];
        [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];

        __block NSData* responseData = nil;
        __block NSError* error = nil;
        __block NSInteger statusCode = 0;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
                responseData = data;
                error = err;
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = [(NSHTTPURLResponse*)response statusCode];
                }
                dispatch_semaphore_signal(sem);
            }];
        [task resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

        if (error || !responseData || statusCode < 200 || statusCode >= 300) {
            return "";
        }
        return std::string(static_cast<const char*>(responseData.bytes), responseData.length);
    }
}

std::string SyncService::HttpGet(const std::string& url, const std::string& token) {
    @autoreleasepool {
        std::string authUrl = WithAuth(url, token);
        NSURL* nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:authUrl.c_str()]];
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:nsUrl];
        request.HTTPMethod = @"GET";

        __block NSData* responseData = nil;
        __block NSError* error = nil;
        __block NSInteger statusCode = 0;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
                responseData = data;
                error = err;
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = [(NSHTTPURLResponse*)response statusCode];
                }
                dispatch_semaphore_signal(sem);
            }];
        [task resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

        if (error || !responseData || statusCode < 200 || statusCode >= 300) {
            return "";
        }
        return std::string(static_cast<const char*>(responseData.bytes), responseData.length);
    }
}

void SyncService::Upload(SyncCallback callback) {
    std::thread([this, callback]() {
        SyncResult result;

        GoogleAuth& auth = GoogleAuth::GetInstance();
        if (!auth.IsSignedIn()) {
            result.success = false;
            result.error_message = "Not signed in";
            if (callback) callback(result);
            return;
        }

        std::string firebaseToken = auth.GetFirebaseToken();
        std::string userId = auth.GetUserId();  // Use Google user ID as path (sanitized)

        if (userId.empty() || firebaseToken.empty()) {
            result.success = false;
            result.error_message = "Invalid authentication - Firebase token missing";
            if (callback) callback(result);
            return;
        }

        // Use Firebase Realtime Database with Authorization header auth.
        std::string baseUrl = firebase_url_.empty()
            ? OAuthConfig::GetFirebaseDatabaseUrl()
            : firebase_url_;

        // Upload bookmarks
        std::string bookmarksJson = RunStringOnMainThread([this]() {
            return ExportBookmarksToJson();
        });
        std::string bookmarksUrl = baseUrl + "/users/" + userId + "/bookmarks.json";
        std::string bookmarksResp = HttpPut(bookmarksUrl, bookmarksJson, firebaseToken);

        if (!bookmarksResp.empty() && bookmarksResp.find("error") == std::string::npos) {
            result.bookmarks_synced = 1;
        }

        // Upload history
        std::string historyJson = RunStringOnMainThread([this]() {
            return ExportHistoryToJson();
        });
        std::string historyUrl = baseUrl + "/users/" + userId + "/history.json";
        std::string historyResp = HttpPut(historyUrl, historyJson, firebaseToken);

        if (!historyResp.empty() && historyResp.find("error") == std::string::npos) {
            result.history_synced = 1;
        }

        // Upload settings
        std::string settingsJson = RunStringOnMainThread([this]() {
            return ExportSettingsToJson();
        });
        std::string settingsUrl = baseUrl + "/users/" + userId + "/settings.json";
        std::string settingsResp = HttpPut(settingsUrl, settingsJson, firebaseToken);

        if (!settingsResp.empty() && settingsResp.find("error") == std::string::npos) {
            result.settings_synced = true;
        }

        // Upload session (workspaces + pinned/open tabs)
        std::string sessionJson = RunStringOnMainThread([this]() {
            return ExportSessionToJson();
        });
        if (!sessionJson.empty()) {
            std::string sessionUrl = baseUrl + "/users/" + userId + "/session.json";
            std::string sessionResp = HttpPut(sessionUrl, sessionJson, firebaseToken);
            if (!sessionResp.empty() && sessionResp.find("error") == std::string::npos) {
                result.session_synced = true;
            }
        }

        result.success = (result.bookmarks_synced > 0 || result.history_synced > 0 ||
                          result.settings_synced || result.session_synced);
        if (!result.success) {
            result.error_message = "Failed to upload data to cloud";
        } else {
            last_sync_time_ = std::time(nullptr);
        }

        if (callback) callback(result);
    }).detach();
}

void SyncService::Download(SyncCallback callback) {
    std::thread([this, callback]() {
        SyncResult result;

        GoogleAuth& auth = GoogleAuth::GetInstance();
        if (!auth.IsSignedIn()) {
            result.success = false;
            result.error_message = "Not signed in";
            if (callback) callback(result);
            return;
        }

        std::string firebaseToken = auth.GetFirebaseToken();
        std::string userId = auth.GetUserId();

        if (userId.empty() || firebaseToken.empty()) {
            result.success = false;
            result.error_message = "Invalid authentication - Firebase token missing";
            if (callback) callback(result);
            return;
        }

        std::string baseUrl = firebase_url_.empty()
            ? OAuthConfig::GetFirebaseDatabaseUrl()
            : firebase_url_;

        // Download and import bookmarks
        std::string bookmarksUrl = baseUrl + "/users/" + userId + "/bookmarks.json";
        std::string bookmarksJson = HttpGet(bookmarksUrl, firebaseToken);

        if (!bookmarksJson.empty() && bookmarksJson != "null") {
            if (RunBoolOnMainThread([this, bookmarksJson]() {
                    return ImportBookmarksFromJson(bookmarksJson);
                })) {
                result.bookmarks_synced = 1;
            }
        }

        // Download and import history
        std::string historyUrl = baseUrl + "/users/" + userId + "/history.json";
        std::string historyJson = HttpGet(historyUrl, firebaseToken);

        if (!historyJson.empty() && historyJson != "null") {
            if (RunBoolOnMainThread([this, historyJson]() {
                    return ImportHistoryFromJson(historyJson);
                })) {
                result.history_synced = 1;
            }
        }

        // Download and import settings
        std::string settingsUrl = baseUrl + "/users/" + userId + "/settings.json";
        std::string settingsJson = HttpGet(settingsUrl, firebaseToken);

        if (!settingsJson.empty() && settingsJson != "null") {
            if (RunBoolOnMainThread([this, settingsJson]() {
                    return ImportSettingsFromJson(settingsJson);
                })) {
                result.settings_synced = true;
            }
        }

        // Download and import session (applied on next launch)
        std::string sessionUrl = baseUrl + "/users/" + userId + "/session.json";
        std::string sessionJson = HttpGet(sessionUrl, firebaseToken);

        if (!sessionJson.empty() && sessionJson != "null") {
            if (RunBoolOnMainThread([this, sessionJson]() {
                    return ImportSessionFromJson(sessionJson);
                })) {
                result.session_synced = true;
            }
        }

        result.success = true;
        last_sync_time_ = std::time(nullptr);

        if (callback) callback(result);
    }).detach();
}

void SyncService::SyncNow(SyncCallback callback) {
    // First upload local data, then download and merge remote data
    Upload([this, callback](const SyncResult& uploadResult) {
        if (!uploadResult.success) {
            if (callback) callback(uploadResult);
            return;
        }

        Download([callback](const SyncResult& downloadResult) {
            SyncResult finalResult = downloadResult;
            finalResult.success = true;
            if (callback) callback(finalResult);
        });
    });
}
