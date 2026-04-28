// Google OAuth 2.0 implementation for desktop applications
// Uses PKCE flow with loopback redirect

#include "sync/google_auth.h"
#include "utils/json_utils.h"

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <array>
#include <map>
#include <cstring>
#include <fstream>
#include <vector>
#include <utility>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// OAuth config loaded from google_oauth_credentials.json (Google's format, not committed to git)
namespace {
    struct OAuthConfigData {
        std::string client_id;
        std::string client_secret;
        std::string project_id;
        std::string auth_uri;
        std::string token_uri;
        std::string redirect_uri;
        std::string firebase_api_key;
        std::string firebase_database_url;
        bool loaded = false;
    };

    // Extract nested JSON value: {"installed":{"client_id":"..."}}
    std::string GetNestedJsonString(const std::string& json, const std::string& outer, const std::string& inner) {
        // Find outer object
        size_t outer_pos = json.find("\"" + outer + "\"");
        if (outer_pos == std::string::npos) return "";

        size_t brace_start = json.find('{', outer_pos);
        if (brace_start == std::string::npos) return "";

        // Find matching closing brace
        int depth = 1;
        size_t brace_end = brace_start + 1;
        while (brace_end < json.size() && depth > 0) {
            if (json[brace_end] == '{') depth++;
            else if (json[brace_end] == '}') depth--;
            brace_end++;
        }

        std::string inner_json = json.substr(brace_start, brace_end - brace_start);
        return orbfox::utils::GetJsonString(inner_json, inner);
    }

    // Get directory containing the executable
    std::string GetExecutableDir() {
#ifdef __APPLE__
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            std::string exePath(path);
            size_t lastSlash = exePath.rfind('/');
            if (lastSlash != std::string::npos) {
                return exePath.substr(0, lastSlash);
            }
        }
#endif
        return ".";
    }

    OAuthConfigData& GetConfigData() {
        static OAuthConfigData config;
        if (!config.loaded) {
            std::string exeDir = GetExecutableDir();

            std::vector<std::string> paths = {
                "google_oauth_credentials.json",  // Current working directory
                exeDir + "/../../../google_oauth_credentials.json",  // From .app/Contents/MacOS/ to project root
                exeDir + "/../../../../google_oauth_credentials.json",  // One more level up
            };

            for (const auto& path : paths) {
                std::ifstream file(path);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
                    // Google's format has "installed" wrapper
                    config.client_id = GetNestedJsonString(content, "installed", "client_id");
                    config.client_secret = GetNestedJsonString(content, "installed", "client_secret");
                    config.project_id = GetNestedJsonString(content, "installed", "project_id");
                    config.auth_uri = GetNestedJsonString(content, "installed", "auth_uri");
                    config.token_uri = GetNestedJsonString(content, "installed", "token_uri");
                    // redirect_uris is an array, just use localhost
                    config.redirect_uri = "http://localhost";
                    // Firebase config (loaded from JSON - required for sync)
                    config.firebase_api_key = GetNestedJsonString(content, "installed", "firebase_api_key");
                    config.firebase_database_url = GetNestedJsonString(content, "installed", "firebase_database_url");
                    config.loaded = true;
                    break;
                }
            }
        }
        return config;
    }
}

namespace OAuthConfig {
    std::string GetClientId() { return GetConfigData().client_id; }
    std::string GetClientSecret() { return GetConfigData().client_secret; }
    std::string GetProjectId() { return GetConfigData().project_id; }
    std::string GetAuthUri() { return GetConfigData().auth_uri; }
    std::string GetTokenUri() { return GetConfigData().token_uri; }
    std::string GetRedirectUri() { return GetConfigData().redirect_uri; }
    std::string GetFirebaseApiKey() { return GetConfigData().firebase_api_key; }
    std::string GetFirebaseDatabaseUrl() { return GetConfigData().firebase_database_url; }
}

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <shellapi.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

// Simple HTTP client using sockets (minimal dependencies)
namespace {

// URL encode a string
std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

// URL decode a string
std::string UrlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int hex = 0;
            std::istringstream iss(value.substr(i + 1, 2));
            if (iss >> std::hex >> hex) {
                result.push_back(static_cast<char>(hex));
                i += 2;
            } else {
                result.push_back(value[i]);
            }
        } else if (value[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(value[i]);
        }
    }
    return result;
}

std::string EscapeHtml(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped.push_back(c); break;
        }
    }
    return escaped;
}

// Base64 URL encode (for PKCE)
std::string Base64UrlEncode(const std::vector<unsigned char>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < data.size()) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < data.size()) n |= static_cast<unsigned int>(data[i + 2]);

        result.push_back(chars[(n >> 18) & 0x3F]);
        result.push_back(chars[(n >> 12) & 0x3F]);
        if (i + 1 < data.size()) result.push_back(chars[(n >> 6) & 0x3F]);
        if (i + 2 < data.size()) result.push_back(chars[n & 0x3F]);
    }
    // No padding for URL-safe base64
    return result;
}

// Simple SHA256 implementation for PKCE code challenge
// (Using a minimal implementation to avoid OpenSSL dependency)
class SHA256 {
public:
    SHA256() { reset(); }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            data_[datalen_++] = data[i];
            if (datalen_ == 64) {
                transform();
                bitlen_ += 512;
                datalen_ = 0;
            }
        }
    }

    void update(const std::string& str) {
        update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    std::vector<unsigned char> final() {
        uint8_t hash[32];
        size_t i = datalen_;

        if (datalen_ < 56) {
            data_[i++] = 0x80;
            while (i < 56) data_[i++] = 0x00;
        } else {
            data_[i++] = 0x80;
            while (i < 64) data_[i++] = 0x00;
            transform();
            memset(data_, 0, 56);
        }

        bitlen_ += datalen_ * 8;
        data_[63] = static_cast<uint8_t>(bitlen_);
        data_[62] = static_cast<uint8_t>(bitlen_ >> 8);
        data_[61] = static_cast<uint8_t>(bitlen_ >> 16);
        data_[60] = static_cast<uint8_t>(bitlen_ >> 24);
        data_[59] = static_cast<uint8_t>(bitlen_ >> 32);
        data_[58] = static_cast<uint8_t>(bitlen_ >> 40);
        data_[57] = static_cast<uint8_t>(bitlen_ >> 48);
        data_[56] = static_cast<uint8_t>(bitlen_ >> 56);
        transform();

        for (i = 0; i < 4; ++i) {
            hash[i]      = (state_[0] >> (24 - i * 8)) & 0xff;
            hash[i + 4]  = (state_[1] >> (24 - i * 8)) & 0xff;
            hash[i + 8]  = (state_[2] >> (24 - i * 8)) & 0xff;
            hash[i + 12] = (state_[3] >> (24 - i * 8)) & 0xff;
            hash[i + 16] = (state_[4] >> (24 - i * 8)) & 0xff;
            hash[i + 20] = (state_[5] >> (24 - i * 8)) & 0xff;
            hash[i + 24] = (state_[6] >> (24 - i * 8)) & 0xff;
            hash[i + 28] = (state_[7] >> (24 - i * 8)) & 0xff;
        }

        return std::vector<unsigned char>(hash, hash + 32);
    }

private:
    void reset() {
        datalen_ = 0;
        bitlen_ = 0;
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
    }

    void transform() {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };

        uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;

        for (int i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (data_[j] << 24) | (data_[j + 1] << 16) | (data_[j + 2] << 8) | data_[j + 3];
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
            uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }

        a = state_[0]; b = state_[1]; c = state_[2]; d = state_[3];
        e = state_[4]; f = state_[5]; g = state_[6]; h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            t1 = h + S1 + ch + k[i] + m[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            t2 = S0 + maj;

            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    static uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    uint8_t data_[64];
    uint32_t datalen_;
    uint64_t bitlen_;
    uint32_t state_[8];
};

// Parse query string from URL (with URL decoding)
std::map<std::string, std::string> ParseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string value = UrlDecode(pair.substr(eq + 1));
            params[key] = value;
        }
    }
    return params;
}

// Extract path and query from HTTP request line
bool ParseRequestLine(const std::string& request, std::string& path, std::string& query) {
    // GET /path?query HTTP/1.1
    size_t method_end = request.find(' ');
    if (method_end == std::string::npos) return false;

    size_t path_end = request.find(' ', method_end + 1);
    if (path_end == std::string::npos) return false;

    std::string full_path = request.substr(method_end + 1, path_end - method_end - 1);
    size_t query_start = full_path.find('?');
    if (query_start != std::string::npos) {
        path = full_path.substr(0, query_start);
        query = full_path.substr(query_start + 1);
    } else {
        path = full_path;
        query = "";
    }
    return true;
}

} // anonymous namespace

// Local server for OAuth callback
class GoogleAuth::LocalServer {
public:
    LocalServer() : socket_(-1), running_(false) {}

    ~LocalServer() {
        Stop();
    }

    bool Start(int& port) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
#endif
        socket_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (socket_ < 0) return false;

        int opt = 1;
#ifdef _WIN32
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        // Try ports in the ephemeral range
        for (port = OAuthConfig::MIN_PORT; port <= OAuthConfig::MAX_PORT; ++port) {
            addr.sin_port = htons(static_cast<uint16_t>(port));
            if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                break;
            }
        }

        if (port > OAuthConfig::MAX_PORT) {
            CloseSocket();
            return false;
        }

        if (listen(socket_, 1) < 0) {
            CloseSocket();
            return false;
        }

        running_ = true;
        return true;
    }

    void Stop() {
        running_ = false;
        CloseSocket();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Wait for OAuth callback, returns authorization code
    std::string WaitForCallback(const std::string& expected_state, std::string& error) {
        if (!running_ || socket_ < 0) {
            error = "Server not running";
            return "";
        }

        // Set socket timeout
#ifdef _WIN32
        DWORD timeout = 120000; // 2 minutes
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 120;
        tv.tv_usec = 0;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        int client = static_cast<int>(accept(socket_, nullptr, nullptr));
        if (client < 0) {
            error = "Timeout waiting for callback";
            return "";
        }

        // Read request
        std::array<char, 4096> buffer{};
        int bytes_read;
#ifdef _WIN32
        bytes_read = recv(client, buffer.data(), static_cast<int>(buffer.size()) - 1, 0);
#else
        bytes_read = static_cast<int>(read(client, buffer.data(), buffer.size() - 1));
#endif

        std::string code;
        if (bytes_read > 0) {
            buffer[static_cast<size_t>(bytes_read)] = '\0';
            std::string request(buffer.data());

            std::string path, query;
            if (ParseRequestLine(request, path, query)) {
                auto params = ParseQueryString(query);

                // Check state matches
                if (params["state"] != expected_state) {
                    error = "State mismatch - possible CSRF attack";
                } else if (params.count("error")) {
                    error = params["error"];
                    if (params.count("error_description")) {
                        error += ": " + params["error_description"];
                    }
                } else if (params.count("code")) {
                    code = params["code"];
                } else {
                    error = "No authorization code in response";
                }
            }
        }

        // Send response to browser
        std::string response_body;
        if (!code.empty()) {
            response_body = R"(
<!DOCTYPE html>
<html>
<head><title>OrbFox - Sign In Successful</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; display: flex;
       justify-content: center; align-items: center; height: 100vh; margin: 0;
       background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%); color: white; }
.container { text-align: center; padding: 40px; }
h1 { color: #34C759; margin-bottom: 10px; }
p { color: #888; }
</style>
</head>
<body>
<div class="container">
<h1>Sign In Successful</h1>
<p>You can close this window and return to OrbFox.</p>
</div>
</body>
</html>
)";
        } else {
            std::string escaped_error = EscapeHtml(error.empty() ? "Unknown error" : error);
            response_body = R"(
<!DOCTYPE html>
<html>
<head><title>OrbFox - Sign In Failed</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; display: flex;
       justify-content: center; align-items: center; height: 100vh; margin: 0;
       background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%); color: white; }
.container { text-align: center; padding: 40px; }
h1 { color: #FF6B6B; margin-bottom: 10px; }
p { color: #888; }
</style>
</head>
<body>
<div class="container">
<h1>Sign In Failed</h1>
<p>)" + escaped_error + R"(</p>
<p>Please close this window and try again.</p>
</div>
</body>
</html>
)";
        }

        std::string http_response = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
                                    "Connection: close\r\n"
                                    "\r\n" + response_body;

#ifdef _WIN32
        send(client, http_response.c_str(), static_cast<int>(http_response.size()), 0);
        closesocket(client);
#else
        write(client, http_response.c_str(), http_response.size());
        close(client);
#endif

        return code;
    }

private:
    void CloseSocket() {
        if (socket_ >= 0) {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = -1;
        }
    }

    int socket_;
    std::atomic<bool> running_;
};

// GoogleAuth implementation

GoogleAuth& GoogleAuth::GetInstance() {
    static GoogleAuth instance;
    return instance;
}

GoogleAuth::GoogleAuth() : server_(std::make_unique<LocalServer>()) {
    // Try to load existing tokens on startup
    tokens_ = LoadTokens();
    profile_ = LoadUserProfile();
    is_signed_in_ = !tokens_.refresh_token.empty();
}

GoogleAuth::~GoogleAuth() {
    StopLocalServer();
    if (auth_thread_.joinable()) {
        auth_thread_.join();
    }
}

std::string GoogleAuth::GenerateCodeVerifier() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<unsigned char> random_bytes(32);
    for (auto& byte : random_bytes) {
        byte = static_cast<unsigned char>(dis(gen));
    }
    return Base64UrlEncode(random_bytes);
}

std::string GoogleAuth::GenerateCodeChallenge(const std::string& verifier) {
    SHA256 sha;
    sha.update(verifier);
    return Base64UrlEncode(sha.final());
}

std::string GoogleAuth::GenerateState() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<unsigned char> random_bytes(16);
    for (auto& byte : random_bytes) {
        byte = static_cast<unsigned char>(dis(gen));
    }
    return Base64UrlEncode(random_bytes);
}

bool GoogleAuth::StartLocalServer(int& port) {
    return server_->Start(port);
}

void GoogleAuth::StopLocalServer() {
    server_->Stop();
}

std::string GoogleAuth::BuildAuthUrl(int port, const std::string& state,
                                      const std::string& code_challenge) {
    std::ostringstream url;
    url << OAuthConfig::GetAuthUri()
        << "?client_id=" << UrlEncode(OAuthConfig::GetClientId())
        << "&redirect_uri=" << UrlEncode("http://localhost:" + std::to_string(port) + "/oauth/callback")
        << "&response_type=code"
        << "&scope=" << UrlEncode(OAuthConfig::SCOPES)
        << "&state=" << UrlEncode(state)
        << "&code_challenge=" << UrlEncode(code_challenge)
        << "&code_challenge_method=S256"
        << "&access_type=offline"
        << "&prompt=consent";
    return url.str();
}

void GoogleAuth::StartSignIn(AuthCallback callback) {
    if (auth_thread_.joinable()) {
        if (auth_flow_active_.load()) {
            AuthResult result;
            result.success = false;
            result.error_message = "Sign-in already in progress";
            callback(result);
            return;
        }
        auth_thread_.join();
    }

    if (OAuthConfig::GetClientId().empty()) {
        AuthResult result;
        result.success = false;
        result.error_message = "OAuth credentials not found. Place google_oauth_credentials.json in project root.";
        callback(result);
        return;
    }

    // Start local server for callback
    int port;
    if (!StartLocalServer(port)) {
        AuthResult result;
        result.success = false;
        result.error_message = "Failed to start local OAuth server";
        callback(result);
        return;
    }

    // Generate PKCE values
    std::string code_verifier = GenerateCodeVerifier();
    std::string state = GenerateState();
    std::string code_challenge = GenerateCodeChallenge(code_verifier);

    // Build auth URL and open in browser
    std::string auth_url = BuildAuthUrl(port, state, code_challenge);

    // Use custom URL opener if set (opens in OrbFox), otherwise use system default
    std::function<void(const std::string&)> url_opener;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        url_opener = url_opener_;
    }

    if (url_opener) {
        url_opener(auth_url);
    } else {
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", auth_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        std::string cmd = "open \"" + auth_url + "\"";
        system(cmd.c_str());
#else
        std::string cmd = "xdg-open \"" + auth_url + "\"";
        system(cmd.c_str());
#endif
    }

    // Wait for callback in background thread
    auth_flow_active_ = true;
    auth_thread_ = std::thread([this, port, callback = std::move(callback),
                                state = std::move(state),
                                code_verifier = std::move(code_verifier)]() mutable {
        std::string error;
        std::string code = server_->WaitForCallback(state, error);
        StopLocalServer();

        if (code.empty()) {
            AuthResult result;
            result.success = false;
            result.error_message = error.empty() ? "No authorization code received" : error;
            callback(result);
            auth_flow_active_ = false;
            return;
        }

        // Exchange code for tokens
        ExchangeCodeForTokens(code, code_verifier, port, callback);
        auth_flow_active_ = false;
    });
}

void GoogleAuth::SignOut() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = AuthTokens{};
    profile_ = UserProfile{};
    is_signed_in_ = false;

    ClearTokens();
    ClearUserProfile();

    if (on_auth_state_changed_) {
        on_auth_state_changed_(false);
    }
}

bool GoogleAuth::IsSignedIn() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_signed_in_;
}

std::string GoogleAuth::GetAccessToken() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_signed_in_) return "";

    if (tokens_.IsExpired()) {
        if (!RefreshAccessToken()) {
            return "";
        }
    }
    return tokens_.access_token;
}

std::string GoogleAuth::GetUserId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profile_.user_id;
}

UserProfile GoogleAuth::GetUserProfile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profile_;
}

std::string GoogleAuth::GetIdToken() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_signed_in_) return "";

    if (tokens_.IsExpired()) {
        if (!RefreshAccessToken()) {
            return "";
        }
    }
    return tokens_.id_token;
}

void GoogleAuth::SetOnAuthStateChanged(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_auth_state_changed_ = std::move(callback);
}

void GoogleAuth::SetUrlOpener(std::function<void(const std::string&)> opener) {
    std::lock_guard<std::mutex> lock(mutex_);
    url_opener_ = std::move(opener);
}

// Token exchange, refresh, and profile fetch are implemented in platform-specific files:
// - google_auth_mac.mm (macOS) - uses NSURLSession and Keychain
// - google_auth_win.cpp (Windows) - uses WinHTTP and Credential Manager
//
// Platform files also implement:
// - StoreTokens / LoadTokens / ClearTokens
// - StoreUserProfile / LoadUserProfile / ClearUserProfile
// - ExchangeCodeForTokens
// - RefreshAccessToken
// - FetchUserProfile
