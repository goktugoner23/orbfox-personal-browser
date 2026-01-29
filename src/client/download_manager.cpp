#include "download_manager.h"
#include "utils/filesystem_utils.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string GetDownloadsFilePath() {
    return orbfox::utils::GetAppSupportPath() + "/downloads.json";
}

// Simple JSON escape
std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default: o << c; break;
        }
    }
    return o.str();
}

// Simple JSON unescape
std::string UnescapeJson(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"': result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'b': result += '\b'; ++i; break;
                case 'f': result += '\f'; ++i; break;
                case 'n': result += '\n'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case 't': result += '\t'; ++i; break;
                default: result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

// Extract string value from JSON key
std::string ExtractJsonString(const std::string& json, const std::string& key) {
    // Try with space after colon first (matches our SaveToDisk format)
    std::string pattern = "\"" + key + "\": \"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        // Fallback to no space
        pattern = "\"" + key + "\":\"";
        pos = json.find(pattern);
        if (pos == std::string::npos) return "";
    }
    pos += pattern.size();
    size_t end = json.find("\"", pos);
    while (end != std::string::npos && end > 0 && json[end - 1] == '\\') {
        end = json.find("\"", end + 1);
    }
    if (end == std::string::npos) return "";
    return UnescapeJson(json.substr(pos, end - pos));
}

// Extract int64 value from JSON key
int64_t ExtractJsonInt64(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return 0;
    pos += pattern.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    // Read number
    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
        num += json[pos++];
    }
    return num.empty() ? 0 : std::stoll(num);
}

}  // namespace

DownloadManager& DownloadManager::GetInstance() {
    static DownloadManager instance;
    return instance;
}

void DownloadManager::UpdateDownload(const DownloadItem& item) {
    DownloadUpdateCallback callback_copy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find existing download
        auto it = std::find_if(downloads_.begin(), downloads_.end(),
            [&item](const DownloadItem& d) { return d.id == item.id; });

        if (it != downloads_.end()) {
            // Preserve start_time and original_url from existing item
            DownloadItem updated = item;
            if (updated.start_time == 0) {
                updated.start_time = it->start_time;
            }
            if (updated.original_url.empty() && !it->original_url.empty()) {
                updated.original_url = it->original_url;
            }
            *it = updated;
        } else {
            DownloadItem new_item = item;
            if (new_item.start_time == 0) {
                new_item.start_time = std::time(nullptr);
            }
            downloads_.push_back(new_item);
        }

        // Copy callback to call outside lock
        callback_copy = on_update_;
    }

    // Notify UI outside the lock to prevent deadlock
    if (callback_copy) {
        callback_copy();
    }
}

std::vector<DownloadItem> DownloadManager::GetDownloads() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Return copy sorted by start time (most recent first)
    std::vector<DownloadItem> result = downloads_;
    std::sort(result.begin(), result.end(),
        [](const DownloadItem& a, const DownloadItem& b) {
            return a.start_time > b.start_time;
        });
    return result;
}

std::optional<DownloadItem> DownloadManager::GetDownload(uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(downloads_.begin(), downloads_.end(),
        [id](const DownloadItem& d) { return d.id == id; });

    if (it != downloads_.end()) {
        return *it;  // Return copy for thread safety
    }
    return std::nullopt;
}

void DownloadManager::SetCancelCallback(uint32_t id, DownloadCancelCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_callbacks_[id] = std::move(callback);
}

void DownloadManager::CancelDownload(uint32_t id) {
    DownloadCancelCallback cancel_callback;
    DownloadUpdateCallback update_callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = std::find_if(downloads_.begin(), downloads_.end(),
            [id](const DownloadItem& d) { return d.id == id; });

        if (it != downloads_.end()) {
            it->state = DownloadState::Canceled;
            it->end_time = std::time(nullptr);
        }

        // Get cancel callback
        auto cb_it = cancel_callbacks_.find(id);
        if (cb_it != cancel_callbacks_.end()) {
            cancel_callback = cb_it->second;
            cancel_callbacks_.erase(cb_it);
        }

        update_callback = on_update_;
    }

    // Call cancel callback outside lock
    if (cancel_callback) {
        cancel_callback();
    }

    // Notify UI update
    if (update_callback) {
        update_callback();
    }

    // Save to disk
    SaveToDisk();
}

void DownloadManager::PauseDownload(uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(downloads_.begin(), downloads_.end(),
        [id](const DownloadItem& d) { return d.id == id; });

    if (it != downloads_.end() && it->state == DownloadState::InProgress) {
        it->state = DownloadState::Paused;
    }
}

void DownloadManager::ResumeDownload(uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(downloads_.begin(), downloads_.end(),
        [id](const DownloadItem& d) { return d.id == id; });

    if (it != downloads_.end() && it->state == DownloadState::Paused) {
        it->state = DownloadState::InProgress;
    }
}

void DownloadManager::ClearCompleted() {
    DownloadUpdateCallback callback_copy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        downloads_.erase(
            std::remove_if(downloads_.begin(), downloads_.end(),
                [](const DownloadItem& d) {
                    return d.state == DownloadState::Complete ||
                           d.state == DownloadState::Canceled;
                }),
            downloads_.end());

        callback_copy = on_update_;
    }

    if (callback_copy) {
        callback_copy();
    }
}

void DownloadManager::SetUpdateCallback(DownloadUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_update_ = std::move(callback);
}

float DownloadManager::GetOverallProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t total_bytes = 0;
    int64_t received_bytes = 0;
    bool has_active = false;

    for (const auto& d : downloads_) {
        if (d.state == DownloadState::InProgress || d.state == DownloadState::Paused) {
            has_active = true;
            if (d.total_bytes > 0) {
                total_bytes += d.total_bytes;
                received_bytes += d.received_bytes;
            }
        }
    }

    if (!has_active) {
        return -1.0f;  // No active downloads
    }

    if (total_bytes == 0) {
        return -2.0f;  // Active downloads but unknown size - don't show progress
    }

    return static_cast<float>(received_bytes) / static_cast<float>(total_bytes);
}

bool DownloadManager::HasActiveDownloads() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& d : downloads_) {
        if (d.state == DownloadState::InProgress || d.state == DownloadState::Paused) {
            return true;
        }
    }
    return false;
}

void DownloadManager::SetPendingOriginalUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_original_url_ = url;
}

std::string DownloadManager::GetAndClearPendingOriginalUrl() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string url = std::move(pending_original_url_);
    pending_original_url_.clear();
    return url;
}

void DownloadManager::SetIsRestart(bool is_restart) {
    std::lock_guard<std::mutex> lock(mutex_);
    is_restart_ = is_restart;
}

bool DownloadManager::GetAndClearIsRestart() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool result = is_restart_;
    is_restart_ = false;
    return result;
}

void DownloadManager::RemoveDownload(uint32_t id) {
    DownloadUpdateCallback callback_copy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        downloads_.erase(
            std::remove_if(downloads_.begin(), downloads_.end(),
                [id](const DownloadItem& d) { return d.id == id; }),
            downloads_.end());

        // Also remove cancel callback
        cancel_callbacks_.erase(id);

        callback_copy = on_update_;
    }

    if (callback_copy) {
        callback_copy();
    }

    // Save after removal
    SaveToDisk();
}

void DownloadManager::LoadFromDisk(const std::string& custom_path) {
    std::string path = custom_path.empty() ? GetDownloadsFilePath() : custom_path;
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Parse JSON array
    std::lock_guard<std::mutex> lock(mutex_);
    downloads_.clear();

    // Find each object in the array
    size_t pos = 0;
    while ((pos = content.find("{", pos)) != std::string::npos) {
        size_t end = content.find("}", pos);
        if (end == std::string::npos) break;

        std::string obj = content.substr(pos, end - pos + 1);

        DownloadItem item;
        item.id = static_cast<uint32_t>(ExtractJsonInt64(obj, "id"));
        item.url = ExtractJsonString(obj, "url");
        item.original_url = ExtractJsonString(obj, "original_url");
        // Fallback: if original_url is empty, use url
        if (item.original_url.empty()) {
            item.original_url = item.url;
        }
        item.filename = ExtractJsonString(obj, "filename");
        item.full_path = ExtractJsonString(obj, "full_path");
        item.mime_type = ExtractJsonString(obj, "mime_type");
        item.total_bytes = ExtractJsonInt64(obj, "total_bytes");
        item.received_bytes = ExtractJsonInt64(obj, "received_bytes");
        item.percent_complete = static_cast<int>(ExtractJsonInt64(obj, "percent_complete"));
        item.start_time = static_cast<std::time_t>(ExtractJsonInt64(obj, "start_time"));
        item.end_time = static_cast<std::time_t>(ExtractJsonInt64(obj, "end_time"));

        int state = static_cast<int>(ExtractJsonInt64(obj, "state"));
        // Validate enum value before casting (0=InProgress, 1=Complete, 2=Canceled, 3=Interrupted, 4=Paused)
        if (state >= 0 && state <= 4) {
            item.state = static_cast<DownloadState>(state);
        } else {
            item.state = DownloadState::Canceled;  // Default to canceled for invalid values
        }

        // Only load completed or canceled downloads (not in-progress ones)
        if (item.state == DownloadState::Complete ||
            item.state == DownloadState::Canceled ||
            item.state == DownloadState::Interrupted) {
            downloads_.push_back(item);
        }

        pos = end + 1;
    }
}

void DownloadManager::SaveToDisk(const std::string& custom_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream json;
    json << "[\n";

    bool first = true;
    for (const auto& d : downloads_) {
        // Only save completed or stopped downloads (not in-progress)
        if (d.state != DownloadState::Complete &&
            d.state != DownloadState::Canceled &&
            d.state != DownloadState::Interrupted) {
            continue;
        }

        if (!first) json << ",\n";
        first = false;

        json << "  {\n";
        json << "    \"id\": " << d.id << ",\n";
        json << "    \"url\": \"" << EscapeJson(d.url) << "\",\n";
        json << "    \"original_url\": \"" << EscapeJson(d.original_url) << "\",\n";
        json << "    \"filename\": \"" << EscapeJson(d.filename) << "\",\n";
        json << "    \"full_path\": \"" << EscapeJson(d.full_path) << "\",\n";
        json << "    \"mime_type\": \"" << EscapeJson(d.mime_type) << "\",\n";
        json << "    \"total_bytes\": " << d.total_bytes << ",\n";
        json << "    \"received_bytes\": " << d.received_bytes << ",\n";
        json << "    \"percent_complete\": " << d.percent_complete << ",\n";
        json << "    \"state\": " << static_cast<int>(d.state) << ",\n";
        json << "    \"start_time\": " << d.start_time << ",\n";
        json << "    \"end_time\": " << d.end_time << "\n";
        json << "  }";
    }

    json << "\n]\n";

    std::string path = custom_path.empty() ? GetDownloadsFilePath() : custom_path;
    std::ofstream file(path);
    if (file.is_open()) {
        file << json.str();
        file.close();
    }
}

void DownloadManager::ResetForTesting() {
    std::lock_guard<std::mutex> lock(mutex_);
    downloads_.clear();
    cancel_callbacks_.clear();
    on_update_ = nullptr;
    pending_original_url_.clear();
    is_restart_ = false;
}
