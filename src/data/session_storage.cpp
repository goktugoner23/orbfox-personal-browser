#include "session_storage.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void EnsureDirectoryExists(const std::string& path) {
#ifdef __APPLE__
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
#endif
}

// Simple JSON escaping
std::string EscapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Simple JSON string extraction
std::string ExtractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.length();
    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

bool ExtractBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    return json.substr(pos, 4) == "true";
}

int ExtractInt(const std::string& json, const std::string& key, int defaultValue = 0) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultValue;
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    std::string num;
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-')) {
        num += json[pos++];
    }
    return num.empty() ? defaultValue : std::stoi(num);
}

}  // namespace

std::string SessionStorage::GetSessionPath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    std::string app_support = std::string(home) + "/Library/Application Support/OrbFox";
    EnsureDirectoryExists(app_support);
    return app_support + "/session.json";
#else
    return "session.json";
#endif
}

void SessionStorage::Save(const SavedSession& session) {
    std::ofstream file(GetSessionPath());
    if (!file.is_open()) return;

    file << "{\n";
    file << "  \"active_workspace_index\": " << session.active_workspace_index << ",\n";
    file << "  \"workspaces\": [\n";

    for (size_t wi = 0; wi < session.workspaces.size(); ++wi) {
        const auto& ws = session.workspaces[wi];
        file << "    {\n";
        file << "      \"name\": \"" << EscapeJson(ws.name) << "\",\n";
        file << "      \"active_tab_index\": " << ws.active_tab_index << ",\n";
        file << "      \"tabs\": [\n";

        for (size_t ti = 0; ti < ws.tabs.size(); ++ti) {
            const auto& tab = ws.tabs[ti];
            file << "        {\n";
            file << "          \"url\": \"" << EscapeJson(tab.url) << "\",\n";
            file << "          \"title\": \"" << EscapeJson(tab.title) << "\",\n";
            file << "          \"is_pinned\": " << (tab.is_pinned ? "true" : "false") << ",\n";
            file << "          \"is_muted\": " << (tab.is_muted ? "true" : "false") << "\n";
            file << "        }";
            if (ti < ws.tabs.size() - 1) file << ",";
            file << "\n";
        }

        file << "      ]\n";
        file << "    }";
        if (wi < session.workspaces.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

SavedSession SessionStorage::Load() {
    SavedSession session;

    std::ifstream file(GetSessionPath());
    if (!file.is_open()) return session;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    session.active_workspace_index = ExtractInt(json, "active_workspace_index", 0);

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
            ws.name = ExtractString(wsJson, "name");
            ws.active_tab_index = ExtractInt(wsJson, "active_tab_index", 0);

            // Parse tabs within this workspace
            size_t tabPos = 0;
            while ((tabPos = wsJson.find("\"url\":", tabPos)) != std::string::npos) {
                // Find the containing object
                size_t objStart = wsJson.rfind("{", tabPos);
                size_t objEnd = wsJson.find("}", tabPos);
                if (objStart != std::string::npos && objEnd != std::string::npos) {
                    std::string tabJson = wsJson.substr(objStart, objEnd - objStart + 1);

                    SavedTab tab;
                    tab.url = ExtractString(tabJson, "url");
                    tab.title = ExtractString(tabJson, "title");
                    tab.is_pinned = ExtractBool(tabJson, "is_pinned");
                    tab.is_muted = ExtractBool(tabJson, "is_muted");

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

bool SessionStorage::HasSavedSession() {
    std::ifstream file(GetSessionPath());
    return file.good();
}
