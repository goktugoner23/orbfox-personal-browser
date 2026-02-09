#pragma once

#include <string>
#include <functional>
#include <ctime>

// User profile from Google account
struct UserProfile {
    std::string user_id;
    std::string email;
    std::string display_name;
    std::string avatar_url;
};

// OAuth tokens
struct AuthTokens {
    std::string access_token;
    std::string refresh_token;
    std::string id_token;           // Google ID token
    std::string firebase_token;     // Firebase ID token (for database access)
    std::string firebase_user_id;   // Firebase user ID
    std::time_t expires_at = 0;

    bool IsExpired() const {
        return std::time(nullptr) >= expires_at - 60; // 60 second buffer
    }
};

// Authentication result
struct AuthResult {
    bool success = false;
    std::string error_message;
    UserProfile profile;
    AuthTokens tokens;
};

// Sync status for UI
enum class SyncStatus {
    SignedOut,      // Not logged in
    Syncing,        // Active sync in progress
    Synced,         // Up to date
    Offline,        // No network, using cached data
    Error           // Sync failed
};

// Callback types
using AuthCallback = std::function<void(const AuthResult&)>;
using SyncStatusCallback = std::function<void(SyncStatus)>;
