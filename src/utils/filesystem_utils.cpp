#include "utils/filesystem_utils.h"

#include <iomanip>
#include <sstream>
#include <string>

#ifdef __APPLE__
#include <sys/stat.h>
#include <cstdlib>  // realpath, getenv
#include <pwd.h>
#include <unistd.h>
#endif

namespace orbfox {
namespace utils {

void EnsureDirectoryExists(const std::string& path) {
#ifdef __APPLE__
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        mkdir(path.c_str(), 0755);
    }
#else
    (void)path;  // Unused on non-Apple platforms
#endif
}

bool IsValidDownloadPath(const std::string& path, bool check_exists) {
    // Empty path is invalid
    if (path.empty()) {
        return false;
    }

    // Check for null bytes (could be used to truncate path)
    if (path.find('\0') != std::string::npos) {
        return false;
    }

    // Check for path traversal sequences
    if (path.find("..") != std::string::npos) {
        return false;
    }

    // Must be absolute path (starts with /) or home path (starts with ~)
    if (path[0] != '/' && path[0] != '~') {
        return false;
    }

    // Check for suspicious patterns - only for absolute paths
    // (~ paths will be expanded and checked after expansion if needed)
    if (path[0] == '/') {
        // Prevent writing to system directories
        if (path.find("/etc/") == 0 ||
            path.find("/usr/") == 0 ||
            path.find("/bin/") == 0 ||
            path.find("/sbin/") == 0 ||
            path.find("/var/") == 0 ||
            path.find("/System/") == 0 ||
            path.find("/Library/") == 0) {
            // Allow ~/Library/... paths for app-specific storage
            // but block /Library (system-wide)
            return false;
        }
    }

    if (check_exists) {
#ifdef __APPLE__
        // Resolve symlinks and verify it's a real directory
        char resolved[PATH_MAX];
        if (realpath(path.c_str(), resolved) == nullptr) {
            return false;  // Path doesn't exist or can't be resolved
        }

        struct stat st;
        if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return false;  // Not a directory
        }
#endif
    }

    return true;
}

std::string EscapeHtml(const std::string& str) {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
            case '<': escaped << "&lt;"; break;
            case '>': escaped << "&gt;"; break;
            case '&': escaped << "&amp;"; break;
            case '"': escaped << "&quot;"; break;
            case '\'': escaped << "&#39;"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

std::string GetHomeDirectory() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    return std::string(home);
#else
    return "/tmp";
#endif
}

std::string GetAppSupportPath() {
#ifdef __APPLE__
    std::string home = GetHomeDirectory();
    std::string app_support = home + "/Library/Application Support/OrbFox";
    EnsureDirectoryExists(app_support);
    return app_support;
#else
    return ".";
#endif
}

std::string UrlEncode(const std::string& str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex << std::uppercase;

    for (unsigned char c : str) {
        // Keep alphanumeric and - _ . ~ unchanged
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            // Percent-encode everything else
            encoded << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return encoded.str();
}

bool AtomicWriteFile(const std::string& path, const std::string& content) {
#ifdef __APPLE__
    // Write to temp file first
    std::string temp_path = path + ".tmp";

    // Write content to temp file
    std::FILE* file = std::fopen(temp_path.c_str(), "w");
    if (!file) {
        return false;
    }

    size_t written = std::fwrite(content.data(), 1, content.size(), file);
    bool write_ok = (written == content.size());

    // Flush to disk before closing
    if (write_ok) {
        write_ok = (std::fflush(file) == 0);
    }

    std::fclose(file);

    if (!write_ok) {
        std::remove(temp_path.c_str());
        return false;
    }

    // Atomic rename (POSIX guarantees this is atomic on same filesystem)
    if (std::rename(temp_path.c_str(), path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }

    return true;
#else
    // Fallback for non-Apple platforms: direct write
    std::FILE* file = std::fopen(path.c_str(), "w");
    if (!file) {
        return false;
    }
    size_t written = std::fwrite(content.data(), 1, content.size(), file);
    std::fclose(file);
    return written == content.size();
#endif
}

}  // namespace utils
}  // namespace orbfox
