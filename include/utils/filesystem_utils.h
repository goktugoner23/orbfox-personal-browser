#ifndef ORBFOX_UTILS_FILESYSTEM_UTILS_H_
#define ORBFOX_UTILS_FILESYSTEM_UTILS_H_

#include <string>

namespace orbfox {
namespace utils {

/// Ensures that a directory exists at the given path.
/// Creates the directory with permissions 0755 if it doesn't exist.
/// On non-Apple platforms, this is currently a no-op.
void EnsureDirectoryExists(const std::string& path);

/// Validates that a path is safe for use as a download directory.
/// Returns true if the path:
/// - Does not contain path traversal sequences (..)
/// - Is an absolute path
/// - Does not contain null bytes
/// - Resolves to a real directory (if check_exists is true)
[[nodiscard]] bool IsValidDownloadPath(const std::string& path, bool check_exists = false);

/// Escapes a string for safe HTML output.
/// Converts: < > & " ' to HTML entities
std::string EscapeHtml(const std::string& str);

/// Gets the OrbFox application support directory path.
/// Returns ~/Library/Application Support/OrbFox on macOS.
/// Creates the directory if it doesn't exist.
std::string GetAppSupportPath();

/// Gets the user's home directory path.
/// Falls back to /tmp if HOME is not set and getpwuid fails.
std::string GetHomeDirectory();

}  // namespace utils
}  // namespace orbfox

#endif  // ORBFOX_UTILS_FILESYSTEM_UTILS_H_
