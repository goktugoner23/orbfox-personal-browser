#pragma once

#include "sync_types.h"
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

// Google OAuth 2.0 authentication for desktop applications
// Uses loopback redirect (localhost) for OAuth callback
class GoogleAuth {
public:
    static GoogleAuth& GetInstance();

    // Start the OAuth sign-in flow
    // Opens browser for Google sign-in, starts local server for callback
    void StartSignIn(AuthCallback callback);

    // Sign out and clear stored tokens
    void SignOut();

    // Check if user is signed in (has valid or refreshable tokens)
    bool IsSignedIn() const;

    // Get current access token (refreshes if needed)
    // Returns empty string if not signed in or refresh fails
    std::string GetAccessToken();

    // Get user ID (Firebase UID)
    std::string GetUserId() const;

    // Get user profile information
    UserProfile GetUserProfile() const;

    // Get ID token for Firebase auth
    std::string GetIdToken();

    // Get Firebase ID token (exchanged from Google token)
    std::string GetFirebaseToken();

    // Set callback for auth state changes
    void SetOnAuthStateChanged(std::function<void(bool signedIn)> callback);

    // Set callback to open URLs in the browser (instead of default system browser)
    void SetUrlOpener(std::function<void(const std::string& url)> opener);

private:
    GoogleAuth();
    ~GoogleAuth();
    GoogleAuth(const GoogleAuth&) = delete;
    GoogleAuth& operator=(const GoogleAuth&) = delete;

    // OAuth flow helpers
    std::string GenerateCodeVerifier();
    std::string GenerateCodeChallenge(const std::string& verifier);
    std::string GenerateState();
    bool StartLocalServer(int& port);
    void StopLocalServer();
    std::string BuildAuthUrl(int port, const std::string& state,
                             const std::string& code_challenge);
    void ExchangeCodeForTokens(const std::string& code,
                               const std::string& code_verifier,
                               int port,
                               AuthCallback callback);
    void FetchUserProfile(AuthCallback callback);
    bool RefreshAccessToken();
    bool ExchangeGoogleTokenForFirebase();

    // Platform-specific token storage (implemented in platform files)
    bool StoreTokens(const AuthTokens& tokens);
    AuthTokens LoadTokens();
    void ClearTokens();

    // Platform-specific user profile storage
    bool StoreUserProfile(const UserProfile& profile);
    UserProfile LoadUserProfile();
    void ClearUserProfile();

    mutable std::mutex mutex_;
    AuthTokens tokens_;
    UserProfile profile_;
    bool is_signed_in_ = false;
    std::function<void(bool)> on_auth_state_changed_;
    std::function<void(const std::string&)> url_opener_;

    // OAuth server state
    class LocalServer;
    std::unique_ptr<LocalServer> server_;
    std::thread auth_thread_;
    std::atomic<bool> auth_flow_active_{false};
};

// OAuth configuration - loaded from google_oauth_credentials.json (not committed to git)
namespace OAuthConfig {
    // From Google's credentials JSON
    std::string GetClientId();
    std::string GetClientSecret();
    std::string GetProjectId();
    std::string GetAuthUri();
    std::string GetTokenUri();
    std::string GetRedirectUri();  // http://localhost

    // Firebase config
    std::string GetFirebaseApiKey();
    std::string GetFirebaseDatabaseUrl();

    // Fixed endpoints
    constexpr const char* USERINFO_ENDPOINT = "https://www.googleapis.com/oauth2/v3/userinfo";
    constexpr const char* FIREBASE_AUTH_ENDPOINT = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithIdp";
    constexpr const char* SCOPES = "openid email profile";

    // Local server port range (appended to redirect_uri)
    constexpr int MIN_PORT = 49152;
    constexpr int MAX_PORT = 65535;
}
