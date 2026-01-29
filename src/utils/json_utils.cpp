#include "utils/json_utils.h"

#include <cctype>
#include <stdexcept>

namespace orbfox {
namespace utils {

namespace {

// Skip whitespace characters in JSON
void SkipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.length() &&
           (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
}

// Find the position after "key": in the JSON
// Returns std::string::npos if not found
size_t FindKeyValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        return std::string::npos;
    }
    pos += search.length();
    SkipWhitespace(json, pos);
    return pos;
}

}  // namespace

std::string GetJsonString(const std::string& json, const std::string& key,
                          const std::string& default_value) {
    size_t pos = FindKeyValue(json, key);
    if (pos == std::string::npos || pos >= json.length()) {
        return default_value;
    }

    // Expect opening quote
    if (json[pos] != '"') {
        return default_value;
    }
    ++pos;  // Skip opening quote

    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            ++pos;
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
        ++pos;
    }
    return result;
}

int GetJsonInt(const std::string& json, const std::string& key, int default_value) {
    size_t pos = FindKeyValue(json, key);
    if (pos == std::string::npos || pos >= json.length()) {
        return default_value;
    }

    // Parse number (including optional negative sign)
    std::string num_str;
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-')) {
        num_str += json[pos++];
    }

    if (num_str.empty()) {
        return default_value;
    }

    try {
        return std::stoi(num_str);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool GetJsonBool(const std::string& json, const std::string& key, bool default_value) {
    size_t pos = FindKeyValue(json, key);
    if (pos == std::string::npos || pos >= json.length()) {
        return default_value;
    }

    if (pos + 4 <= json.length() && json.substr(pos, 4) == "true") {
        return true;
    }
    if (pos + 5 <= json.length() && json.substr(pos, 5) == "false") {
        return false;
    }
    return default_value;
}

std::string EscapeJsonString(const std::string& str) {
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

}  // namespace utils
}  // namespace orbfox
