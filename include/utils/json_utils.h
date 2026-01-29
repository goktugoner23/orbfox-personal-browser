#ifndef ORBFOX_UTILS_JSON_UTILS_H_
#define ORBFOX_UTILS_JSON_UTILS_H_

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

/// Extracts a boolean value from a JSON object.
/// Returns default_value if the key is not found or parsing fails.
bool GetJsonBool(const std::string& json, const std::string& key,
                 bool default_value = false);

/// Escapes a string for JSON output.
/// Handles: " -> \", \ -> \\, newline -> \n, carriage return -> \r, tab -> \t
std::string EscapeJsonString(const std::string& str);

}  // namespace utils
}  // namespace orbfox

#endif  // ORBFOX_UTILS_JSON_UTILS_H_
