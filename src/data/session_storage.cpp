#include "session_storage.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"

namespace {

std::string& StorageDirectoryOverride() {
    static std::string directory;
    return directory;
}

std::string GetStorageDirectory() {
    const std::string& override_directory = StorageDirectoryOverride();
    if (!override_directory.empty()) {
        return override_directory;
    }
    return orbfox::utils::GetAppSupportPath();
}

}  // namespace

std::string SessionStorage::GetSessionPath() {
    return GetStorageDirectory() + "/session.json";
}

void SessionStorage::Save(const SavedSession& session) {
    std::ostringstream ss;

    ss << "{\n";
    ss << "  \"active_workspace_index\": " << session.active_workspace_index << ",\n";
    ss << "  \"workspaces\": [\n";

    for (size_t wi = 0; wi < session.workspaces.size(); ++wi) {
        const auto& ws = session.workspaces[wi];
        ss << "    {\n";
        ss << "      \"name\": \"" << orbfox::utils::EscapeJsonString(ws.name) << "\",\n";
        ss << "      \"color\": \"" << orbfox::utils::EscapeJsonString(ws.color) << "\",\n";
        ss << "      \"active_tab_index\": " << ws.active_tab_index << ",\n";
        ss << "      \"tabs\": [\n";

        for (size_t ti = 0; ti < ws.tabs.size(); ++ti) {
            const auto& tab = ws.tabs[ti];
            ss << "        {\n";
            ss << "          \"url\": \"" << orbfox::utils::EscapeJsonString(tab.url) << "\",\n";
            ss << "          \"title\": \"" << orbfox::utils::EscapeJsonString(tab.title) << "\",\n";
            ss << "          \"is_pinned\": " << (tab.is_pinned ? "true" : "false") << ",\n";
            ss << "          \"is_muted\": " << (tab.is_muted ? "true" : "false") << "\n";
            ss << "        }";
            if (ti < ws.tabs.size() - 1) ss << ",";
            ss << "\n";
        }

        ss << "      ]\n";
        ss << "    }";
        if (wi < session.workspaces.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    // Intentionally ignore return - Save() is best-effort
    (void)orbfox::utils::AtomicWriteFile(GetSessionPath(), ss.str());
}

SavedSession SessionStorage::Load() {
    SavedSession session;

    std::ifstream file(GetSessionPath());
    if (!file.is_open()) return session;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    session.active_workspace_index = orbfox::utils::GetJsonInt(json, "active_workspace_index", 0);

    // Parse workspaces array
    auto workspacesPos = json.find("\"workspaces\":");
    if (workspacesPos == std::string::npos) return session;

    // Find each workspace object
    size_t pos = workspacesPos;
    while ((pos = json.find("{", pos + 1)) != std::string::npos) {
        // Check if this is a workspace or a tab
        auto namePos = json.find("\"name\":", pos);
        auto urlPos = json.find("\"url\":", pos);

        // If "name" comes before "url" or there's no "url", it's a workspace
        if (namePos != std::string::npos && (urlPos == std::string::npos || namePos < urlPos)) {
            // Find the end of this workspace object
            int braceCount = 1;
            size_t endPos = pos + 1;
            while (endPos < json.length() && braceCount > 0) {
                if (json[endPos] == '{') braceCount++;
                else if (json[endPos] == '}') braceCount--;
                endPos++;
            }

            std::string wsJson = json.substr(pos, endPos - pos);

            SavedWorkspace ws;
            ws.name = orbfox::utils::GetJsonString(wsJson, "name");
            ws.color = orbfox::utils::GetJsonString(wsJson, "color");
            ws.active_tab_index = orbfox::utils::GetJsonInt(wsJson, "active_tab_index", 0);

            // Parse tabs within this workspace
            size_t tabPos = 0;
            while ((tabPos = wsJson.find("\"url\":", tabPos)) != std::string::npos) {
                // Find the containing object
                size_t objStart = wsJson.rfind("{", tabPos);
                size_t objEnd = wsJson.find("}", tabPos);
                if (objStart != std::string::npos && objEnd != std::string::npos) {
                    std::string tabJson = wsJson.substr(objStart, objEnd - objStart + 1);

                    SavedTab tab;
                    tab.url = orbfox::utils::GetJsonString(tabJson, "url");
                    tab.title = orbfox::utils::GetJsonString(tabJson, "title");
                    tab.is_pinned = orbfox::utils::GetJsonBool(tabJson, "is_pinned");
                    tab.is_muted = orbfox::utils::GetJsonBool(tabJson, "is_muted");

                    if (!tab.url.empty()) {
                        ws.tabs.push_back(tab);
                    }
                }
                tabPos = objEnd;
            }

            if (!ws.name.empty()) {
                session.workspaces.push_back(ws);
            }

            pos = endPos;
        }
    }

    return session;
}

std::string SessionStorage::ReadRawJson() {
    std::ifstream file(GetSessionPath());
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool SessionStorage::WriteRawJson(const std::string& json) {
    if (json.empty()) return false;
    return orbfox::utils::AtomicWriteFile(GetSessionPath(), json);
}

bool SessionStorage::HasSavedSession() {
    std::ifstream file(GetSessionPath());
    return file.good();
}

void SessionStorage::Clear() {
    std::remove(GetSessionPath().c_str());
}

std::string SessionStorage::GetCrashLockPath() {
    return GetStorageDirectory() + "/running.lock";
}

void SessionStorage::MarkRunning() {
    // Create a lock file to indicate the browser is running
    std::ofstream file(GetCrashLockPath());
    if (file.is_open()) {
        file << "OrbFox is running";
        file.close();
    }
}

void SessionStorage::MarkCleanShutdown() {
    // Remove the lock file on clean shutdown
    std::remove(GetCrashLockPath().c_str());
}

bool SessionStorage::DidCrashLastSession() {
    // If lock file exists, we crashed last time
    std::ifstream file(GetCrashLockPath());
    return file.good();
}

void SessionStorage::AutoSave(const SavedSession& session) {
    // Same as Save(), but called periodically for crash recovery
    Save(session);
}

std::string SessionStorage::GetCrashReportPath() {
    return GetStorageDirectory() + "/crash_report.txt";
}

void SessionStorage::WriteCrashReport(const SavedSession& session) {
    std::ofstream file(GetCrashReportPath());
    if (!file.is_open()) return;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

    file << "OrbFox Crash Report\n";
    file << "==================\n\n";
    file << "Detected: " << timestamp << "\n";
    file << "Platform: macOS\n\n";

    file << "Session State at Crash:\n";
    file << "-----------------------\n";
    file << "Workspaces: " << session.workspaces.size() << "\n";
    file << "Active workspace index: " << session.active_workspace_index << "\n\n";

    int totalTabs = 0;
    for (size_t i = 0; i < session.workspaces.size(); ++i) {
        const auto& ws = session.workspaces[i];
        file << "Workspace " << (i + 1) << ": " << ws.name << "\n";
        file << "  Color: " << ws.color << "\n";
        file << "  Active tab: " << ws.active_tab_index << "\n";
        file << "  Tabs (" << ws.tabs.size() << "):\n";

        for (size_t j = 0; j < ws.tabs.size(); ++j) {
            const auto& tab = ws.tabs[j];
            file << "    " << (j + 1) << ". " << tab.title << "\n";
            file << "       URL: " << tab.url << "\n";
            if (tab.is_pinned) file << "       [Pinned]\n";
            if (tab.is_muted) file << "       [Muted]\n";
        }
        totalTabs += ws.tabs.size();
        file << "\n";
    }

    file << "Total tabs: " << totalTabs << "\n";
    file.close();
}

std::string SessionStorage::GetLastCrashReport() {
    std::ifstream file(GetCrashReportPath());
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void SessionStorage::ClearCrashReports() {
    std::remove(GetCrashReportPath().c_str());
}

void SessionStorage::SetStorageDirectoryForTesting(const std::string& directory) {
    StorageDirectoryOverride() = directory;
    if (!directory.empty()) {
        orbfox::utils::EnsureDirectoryExists(directory);
    }
}

void SessionStorage::ClearStorageDirectoryForTesting() {
    StorageDirectoryOverride().clear();
}
