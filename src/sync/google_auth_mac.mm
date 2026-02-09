// macOS-specific Google OAuth implementation
// Uses NSURLSession for HTTP requests and Keychain for token storage

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "sync/google_auth.h"
#include "utils/json_utils.h"

#include <sstream>

namespace {

// Keychain service name
constexpr const char* KEYCHAIN_SERVICE = "com.orbfox.browser.oauth";
constexpr const char* KEYCHAIN_ACCOUNT_TOKENS = "google_tokens";
constexpr const char* KEYCHAIN_ACCOUNT_PROFILE = "user_profile";

// Store data in Keychain
bool KeychainStore(const std::string& account, const std::string& data) {
    NSString* service = [NSString stringWithUTF8String:KEYCHAIN_SERVICE];
    NSString* acc = [NSString stringWithUTF8String:account.c_str()];
    NSData* dataObj = [NSData dataWithBytes:data.c_str() length:data.size()];

    // Delete existing item first
    NSDictionary* deleteQuery = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: service,
        (__bridge id)kSecAttrAccount: acc
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);

    // Add new item
    NSDictionary* addQuery = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: service,
        (__bridge id)kSecAttrAccount: acc,
        (__bridge id)kSecValueData: dataObj,
        (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleWhenUnlocked
    };

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    return status == errSecSuccess;
}

// Load data from Keychain
std::string KeychainLoad(const std::string& account) {
    NSString* service = [NSString stringWithUTF8String:KEYCHAIN_SERVICE];
    NSString* acc = [NSString stringWithUTF8String:account.c_str()];

    NSDictionary* query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: service,
        (__bridge id)kSecAttrAccount: acc,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
    };

    CFDataRef dataRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&dataRef);

    if (status == errSecSuccess && dataRef) {
        NSData* data = (__bridge_transfer NSData*)dataRef;
        return std::string(static_cast<const char*>(data.bytes), data.length);
    }
    return "";
}

// Delete data from Keychain
void KeychainDelete(const std::string& account) {
    NSString* service = [NSString stringWithUTF8String:KEYCHAIN_SERVICE];
    NSString* acc = [NSString stringWithUTF8String:account.c_str()];

    NSDictionary* query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: service,
        (__bridge id)kSecAttrAccount: acc
    };
    SecItemDelete((__bridge CFDictionaryRef)query);
}

// Synchronous HTTP POST request
std::string HttpPost(const std::string& url, const std::string& body,
                     const std::string& content_type = "application/x-www-form-urlencoded") {
    @autoreleasepool {
        NSURL* nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:nsUrl];
        request.HTTPMethod = @"POST";
        request.HTTPBody = [NSData dataWithBytes:body.c_str() length:body.size()];
        [request setValue:[NSString stringWithUTF8String:content_type.c_str()]
               forHTTPHeaderField:@"Content-Type"];

        __block NSData* responseData = nil;
        __block NSError* error = nil;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
                responseData = data;
                error = err;
                dispatch_semaphore_signal(sem);
            }];
        [task resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

        if (error || !responseData) {
            return "";
        }
        return std::string(static_cast<const char*>(responseData.bytes), responseData.length);
    }
}

// Synchronous HTTP GET request with Authorization header
std::string HttpGet(const std::string& url, const std::string& auth_token) {
    @autoreleasepool {
        NSURL* nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:nsUrl];
        request.HTTPMethod = @"GET";
        if (!auth_token.empty()) {
            [request setValue:[NSString stringWithFormat:@"Bearer %s", auth_token.c_str()]
                   forHTTPHeaderField:@"Authorization"];
        }

        __block NSData* responseData = nil;
        __block NSError* error = nil;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
                responseData = data;
                error = err;
                dispatch_semaphore_signal(sem);
            }];
        [task resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

        if (error || !responseData) {
            return "";
        }
        return std::string(static_cast<const char*>(responseData.bytes), responseData.length);
    }
}

// URL encode helper
std::string UrlEncode(const std::string& value) {
    @autoreleasepool {
        NSString* str = [NSString stringWithUTF8String:value.c_str()];
        NSString* encoded = [str stringByAddingPercentEncodingWithAllowedCharacters:
            [NSCharacterSet URLQueryAllowedCharacterSet]];
        return encoded ? std::string([encoded UTF8String]) : value;
    }
}

// Simple JSON serialization for tokens
std::string TokensToJson(const AuthTokens& tokens) {
    using orbfox::utils::EscapeJsonString;
    std::ostringstream ss;
    ss << "{";
    ss << "\"access_token\":\"" << EscapeJsonString(tokens.access_token) << "\",";
    ss << "\"refresh_token\":\"" << EscapeJsonString(tokens.refresh_token) << "\",";
    ss << "\"id_token\":\"" << EscapeJsonString(tokens.id_token) << "\",";
    ss << "\"firebase_token\":\"" << EscapeJsonString(tokens.firebase_token) << "\",";
    ss << "\"firebase_user_id\":\"" << EscapeJsonString(tokens.firebase_user_id) << "\",";
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
    tokens.firebase_token = GetJsonString(json, "firebase_token", "");
    tokens.firebase_user_id = GetJsonString(json, "firebase_user_id", "");
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
    return KeychainStore(KEYCHAIN_ACCOUNT_TOKENS, TokensToJson(tokens));
}

AuthTokens GoogleAuth::LoadTokens() {
    std::string data = KeychainLoad(KEYCHAIN_ACCOUNT_TOKENS);
    if (data.empty()) return AuthTokens{};
    return TokensFromJson(data);
}

void GoogleAuth::ClearTokens() {
    KeychainDelete(KEYCHAIN_ACCOUNT_TOKENS);
}

bool GoogleAuth::StoreUserProfile(const UserProfile& profile) {
    return KeychainStore(KEYCHAIN_ACCOUNT_PROFILE, ProfileToJson(profile));
}

UserProfile GoogleAuth::LoadUserProfile() {
    std::string data = KeychainLoad(KEYCHAIN_ACCOUNT_PROFILE);
    if (data.empty()) return UserProfile{};
    return ProfileFromJson(data);
}

void GoogleAuth::ClearUserProfile() {
    KeychainDelete(KEYCHAIN_ACCOUNT_PROFILE);
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

        // Store tokens
        StoreTokens(tokens_);
    }

    // Fetch user profile
    FetchUserProfile(callback);
}

bool GoogleAuth::RefreshAccessToken() {
    using namespace orbfox::utils;

    // Build refresh request
    std::ostringstream body;
    body << "refresh_token=" << UrlEncode(tokens_.refresh_token)
         << "&client_id=" << UrlEncode(OAuthConfig::GetClientId())
         << "&client_secret=" << UrlEncode(OAuthConfig::GetClientSecret())
         << "&grant_type=refresh_token";

    std::string response = HttpPost(OAuthConfig::GetTokenUri(), body.str());

    if (response.empty()) {
        return false;
    }

    // Check for error
    std::string error = GetJsonString(response, "error", "");
    if (!error.empty()) {
        // Refresh failed, user needs to sign in again
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

    // Update stored tokens
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

        // Store profile
        StoreUserProfile(profile_);

        is_signed_in_ = true;
    }

    // Exchange Google token for Firebase token (for database access)
    if (!ExchangeGoogleTokenForFirebase()) {
        // Firebase exchange failed, but Google auth succeeded
        // Sync won't work but user is still "signed in" to Google
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        result.success = true;
        result.profile = profile_;
        result.tokens = tokens_;
    }

    if (on_auth_state_changed_) {
        on_auth_state_changed_(true);
    }

    callback(result);
}

std::string GoogleAuth::GetFirebaseToken() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_signed_in_) return "";

    if (tokens_.IsExpired()) {
        if (!RefreshAccessToken()) {
            return "";
        }
    }
    return tokens_.firebase_token;
}

bool GoogleAuth::ExchangeGoogleTokenForFirebase() {
    using namespace orbfox::utils;

    std::string google_id_token = tokens_.id_token;
    if (google_id_token.empty()) {
        return false;
    }

    std::string api_key = OAuthConfig::GetFirebaseApiKey();
    if (api_key.empty()) {
        return false;
    }

    // Build Firebase Auth request
    // POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithIdp?key=[API_KEY]
    std::string url = std::string(OAuthConfig::FIREBASE_AUTH_ENDPOINT) + "?key=" + api_key;

    // Build JSON body
    std::ostringstream body;
    body << "{";
    body << "\"postBody\":\"id_token=" << UrlEncode(google_id_token) << "&providerId=google.com\",";
    body << "\"requestUri\":\"http://localhost\",";
    body << "\"returnIdpCredential\":true,";
    body << "\"returnSecureToken\":true";
    body << "}";

    std::string response = HttpPost(url, body.str(), "application/json");

    if (response.empty()) {
        return false;
    }

    // Check for error
    std::string error = GetJsonString(response, "error", "");
    if (!error.empty()) {
        return false;
    }

    // Extract Firebase tokens
    tokens_.firebase_token = GetJsonString(response, "idToken", "");
    tokens_.firebase_user_id = GetJsonString(response, "localId", "");

    if (tokens_.firebase_token.empty()) {
        return false;
    }

    // Update stored tokens
    StoreTokens(tokens_);
    return true;
}

#endif // __APPLE__
