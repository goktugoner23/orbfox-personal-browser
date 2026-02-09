// Windows-specific Google OAuth implementation
// Uses WinHTTP for HTTP requests and Credential Manager for token storage

#ifdef _WIN32

#include "sync/google_auth.h"
#include "utils/json_utils.h"

#include <windows.h>
#include <wincred.h>
#include <winhttp.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "credui.lib")

namespace {

// Credential Manager target names
constexpr const wchar_t* CRED_TARGET_TOKENS = L"OrbFox/GoogleOAuth/Tokens";
constexpr const wchar_t* CRED_TARGET_PROFILE = L"OrbFox/GoogleOAuth/Profile";

// Store data in Windows Credential Manager
bool CredentialStore(const wchar_t* target, const std::string& data) {
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target);
    cred.CredentialBlobSize = static_cast<DWORD>(data.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(data.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW(&cred, 0) == TRUE;
}

// Load data from Windows Credential Manager
std::string CredentialLoad(const wchar_t* target) {
    PCREDENTIALW cred = nullptr;
    if (CredReadW(target, CRED_TYPE_GENERIC, 0, &cred) && cred) {
        std::string result(reinterpret_cast<char*>(cred->CredentialBlob),
                          cred->CredentialBlobSize);
        CredFree(cred);
        return result;
    }
    return "";
}

// Delete data from Windows Credential Manager
void CredentialDelete(const wchar_t* target) {
    CredDeleteW(target, CRED_TYPE_GENERIC, 0);
}

// URL encode helper
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

// Convert string to wide string
std::wstring ToWideString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

// HTTP POST request using WinHTTP
std::string HttpPost(const std::string& url, const std::string& body,
                     const std::string& content_type = "application/x-www-form-urlencoded") {
    std::wstring wurl = ToWideString(url);

    // Parse URL
    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
        return "";
    }

    HINTERNET hSession = WinHttpOpen(L"OrbFox/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath,
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers = L"Content-Type: " + ToWideString(content_type);
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            const_cast<char*>(body.c_str()),
                            static_cast<DWORD>(body.size()),
                            static_cast<DWORD>(body.size()), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string result;
    DWORD bytesRead = 0;
    char buffer[4096];

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// HTTP GET request with Authorization header
std::string HttpGet(const std::string& url, const std::string& auth_token) {
    std::wstring wurl = ToWideString(url);

    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
        return "";
    }

    HINTERNET hSession = WinHttpOpen(L"OrbFox/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!auth_token.empty()) {
        std::wstring authHeader = L"Authorization: Bearer " + ToWideString(auth_token);
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string result;
    DWORD bytesRead = 0;
    char buffer[4096];

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// Simple JSON serialization for tokens
std::string TokensToJson(const AuthTokens& tokens) {
    using orbfox::utils::EscapeJsonString;
    std::ostringstream ss;
    ss << "{";
    ss << "\"access_token\":\"" << EscapeJsonString(tokens.access_token) << "\",";
    ss << "\"refresh_token\":\"" << EscapeJsonString(tokens.refresh_token) << "\",";
    ss << "\"id_token\":\"" << EscapeJsonString(tokens.id_token) << "\",";
    ss << "\"expires_at\":" << tokens.expires_at;
    ss << "}";
    return ss.str();
}

AuthTokens TokensFromJson(const std::string& json) {
    using namespace orbfox::utils;
    AuthTokens tokens;
    tokens.access_token = GetJsonString(json, "access_token", "");
    tokens.refresh_token = GetJsonString(json, "refresh_token", "");
    tokens.id_token = GetJsonString(json, "id_token", "");
    tokens.expires_at = static_cast<std::time_t>(GetJsonInt(json, "expires_at", 0));
    return tokens;
}

// Simple JSON serialization for profile
std::string ProfileToJson(const UserProfile& profile) {
    using orbfox::utils::EscapeJsonString;
    std::ostringstream ss;
    ss << "{";
    ss << "\"user_id\":\"" << EscapeJsonString(profile.user_id) << "\",";
    ss << "\"email\":\"" << EscapeJsonString(profile.email) << "\",";
    ss << "\"display_name\":\"" << EscapeJsonString(profile.display_name) << "\",";
    ss << "\"avatar_url\":\"" << EscapeJsonString(profile.avatar_url) << "\"";
    ss << "}";
    return ss.str();
}

UserProfile ProfileFromJson(const std::string& json) {
    using namespace orbfox::utils;
    UserProfile profile;
    profile.user_id = GetJsonString(json, "user_id", "");
    profile.email = GetJsonString(json, "email", "");
    profile.display_name = GetJsonString(json, "display_name", "");
    profile.avatar_url = GetJsonString(json, "avatar_url", "");
    return profile;
}

} // anonymous namespace

// Platform-specific token storage

bool GoogleAuth::StoreTokens(const AuthTokens& tokens) {
    return CredentialStore(CRED_TARGET_TOKENS, TokensToJson(tokens));
}

AuthTokens GoogleAuth::LoadTokens() {
    std::string data = CredentialLoad(CRED_TARGET_TOKENS);
    if (data.empty()) return AuthTokens{};
    return TokensFromJson(data);
}

void GoogleAuth::ClearTokens() {
    CredentialDelete(CRED_TARGET_TOKENS);
}

bool GoogleAuth::StoreUserProfile(const UserProfile& profile) {
    return CredentialStore(CRED_TARGET_PROFILE, ProfileToJson(profile));
}

UserProfile GoogleAuth::LoadUserProfile() {
    std::string data = CredentialLoad(CRED_TARGET_PROFILE);
    if (data.empty()) return UserProfile{};
    return ProfileFromJson(data);
}

void GoogleAuth::ClearUserProfile() {
    CredentialDelete(CRED_TARGET_PROFILE);
}

// Platform-specific HTTP implementation for token exchange

void GoogleAuth::ExchangeCodeForTokens(const std::string& code,
                                        const std::string& code_verifier,
                                        int port,
                                        AuthCallback callback) {
    using namespace orbfox::utils;

    // Build token request body
    std::ostringstream body;
    body << "code=" << UrlEncode(code)
         << "&client_id=" << UrlEncode(OAuthConfig::GetClientId())
         << "&client_secret=" << UrlEncode(OAuthConfig::GetClientSecret())
         << "&redirect_uri=" << UrlEncode("http://localhost:" + std::to_string(port) + "/oauth/callback")
         << "&grant_type=authorization_code"
         << "&code_verifier=" << UrlEncode(code_verifier);

    // Make HTTP request
    std::string response = HttpPost(OAuthConfig::GetTokenUri(), body.str());

    if (response.empty()) {
        AuthResult result;
        result.success = false;
        result.error_message = "Failed to exchange authorization code";
        callback(result);
        return;
    }

    // Check for error
    std::string error = GetJsonString(response, "error", "");
    if (!error.empty()) {
        AuthResult result;
        result.success = false;
        result.error_message = GetJsonString(response, "error_description", error);
        callback(result);
        return;
    }

    // Parse tokens
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.access_token = GetJsonString(response, "access_token", "");
        std::string new_refresh = GetJsonString(response, "refresh_token", "");
        if (!new_refresh.empty()) {
            tokens_.refresh_token = new_refresh;
        }
        tokens_.id_token = GetJsonString(response, "id_token", "");

        int expires_in = GetJsonInt(response, "expires_in", 3600);
        tokens_.expires_at = std::time(nullptr) + expires_in;

        StoreTokens(tokens_);
    }

    FetchUserProfile(callback);
}

bool GoogleAuth::RefreshAccessToken() {
    using namespace orbfox::utils;

    std::ostringstream body;
    body << "refresh_token=" << UrlEncode(tokens_.refresh_token)
         << "&client_id=" << UrlEncode(OAuthConfig::GetClientId())
         << "&client_secret=" << UrlEncode(OAuthConfig::GetClientSecret())
         << "&grant_type=refresh_token";

    std::string response = HttpPost(OAuthConfig::GetTokenUri(), body.str());

    if (response.empty()) {
        return false;
    }

    std::string error = GetJsonString(response, "error", "");
    if (!error.empty()) {
        is_signed_in_ = false;
        if (on_auth_state_changed_) {
            on_auth_state_changed_(false);
        }
        return false;
    }

    tokens_.access_token = GetJsonString(response, "access_token", "");
    std::string new_id_token = GetJsonString(response, "id_token", "");
    if (!new_id_token.empty()) {
        tokens_.id_token = new_id_token;
    }

    int expires_in = GetJsonInt(response, "expires_in", 3600);
    tokens_.expires_at = std::time(nullptr) + expires_in;

    StoreTokens(tokens_);
    return true;
}

void GoogleAuth::FetchUserProfile(AuthCallback callback) {
    using namespace orbfox::utils;

    std::string access_token;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        access_token = tokens_.access_token;
    }

    std::string response = HttpGet(OAuthConfig::USERINFO_ENDPOINT, access_token);

    AuthResult result;

    if (response.empty()) {
        result.success = false;
        result.error_message = "Failed to fetch user profile";
        callback(result);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        profile_.user_id = GetJsonString(response, "sub", "");
        profile_.email = GetJsonString(response, "email", "");
        profile_.display_name = GetJsonString(response, "name", "");
        profile_.avatar_url = GetJsonString(response, "picture", "");

        StoreUserProfile(profile_);

        is_signed_in_ = true;

        result.success = true;
        result.profile = profile_;
        result.tokens = tokens_;
    }

    if (on_auth_state_changed_) {
        on_auth_state_changed_(true);
    }

    callback(result);
}

#endif // _WIN32
