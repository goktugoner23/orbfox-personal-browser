#ifndef ORBFOX_UTILS_JSON_UTILS_H_
#define ORBFOX_UTILS_JSON_UTILS_H_

#include <cstdint>
#include <string>

namespace orbfox {
namespace utils {

/// Extracts a string value from a JSON object.
/// Returns default_value if the key is not found or parsing fails.
/// Handles escape sequences: newline, return, tab, quote, backslash
std::string GetJsonString(const std::string& json, const std::string& key,
                          const std::string& default_value = "");

/// Extracts an integer value from a JSON object.
/// Returns default_value if the key is not found or parsing fails.
int GetJsonInt(const std::string& json, const std::string& key,
               int default_value = 0);

/// Extracts a 64-bit integer value from a JSON object (e.g. epoch timestamps).
/// Returns default_value if the key is not found or parsing fails.
int64_t GetJsonInt64(const std::string& json, const std::string& key,
                     int64_t default_value = 0);

/// Extracts a boolean value from a JSON object.
/// Returns default_value if the key is not found or parsing fails.
bool GetJsonBool(const std::string& json, const std::string& key,
                 bool default_value = false);

/// Escapes a string for JSON output.
/// Handles: " -> \", \ -> \\, newline -> \n, carriage return -> \r, tab -> \t
std::string EscapeJsonString(const std::string& str);

/// Extracts the raw content of a JSON array for a given key.
/// Returns the content between [ and ] (exclusive), or empty string if not found.
std::string GetJsonArrayContent(const std::string& json, const std::string& key);

/// Iterates JSON objects within an array string.
/// Starting from pos, finds the next {...} object and returns it.
/// Updates pos to point past the returned object.
/// Returns empty string when no more objects are found.
std::string GetNextJsonObject(const std::string& array_content, size_t& pos);

}  // namespace utils
}  // namespace orbfox

#endif  // ORBFOX_UTILS_JSON_UTILS_H_
