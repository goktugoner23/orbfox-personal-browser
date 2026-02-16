#include "browser_client.h"
#include "download_manager.h"
#include "history_storage.h"
#include "settings_storage.h"
#include "utils/filesystem_utils.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_image.h"
#include "include/wrapper/cef_helpers.h"

#include <sstream>
#include <filesystem>
#include <algorithm>

#if defined(PLATFORM_WIN)
#include <windows.h>
#endif

// Callback for favicon download
class FaviconDownloadCallback : public CefDownloadImageCallback {
public:
    using Callback = std::function<void(const std::string&, const std::vector<unsigned char>&)>;

    FaviconDownloadCallback(std::string url, Callback callback)
        : url_(std::move(url)), callback_(std::move(callback)) {}

    void OnDownloadImageFinished(const CefString& image_url,
                                 int http_status_code,
                                 CefRefPtr<CefImage> image) override {
        if (http_status_code == 200 && image && !image->IsEmpty()) {
            int pixel_width = 0;
            int pixel_height = 0;
            CefRefPtr<CefBinaryValue> png_data = image->GetAsPNG(1.0f, true, pixel_width, pixel_height);

            if (png_data && png_data->GetSize() > 0) {
                std::vector<unsigned char> data(png_data->GetSize());
                png_data->GetData(data.data(), data.size(), 0);

                if (callback_) {
                    callback_(url_, data);
                }
            }
        }
    }

private:
    std::string url_;
    Callback callback_;

    IMPLEMENT_REFCOUNTING(FaviconDownloadCallback);
    DISALLOW_COPY_AND_ASSIGN(FaviconDownloadCallback);
};

// Global context menu suppression for gesture detection
static std::atomic<bool> g_context_menu_suppressed{false};

void SuppressContextMenu(bool suppress) {
    g_context_menu_suppressed.store(suppress);
}

bool IsContextMenuSuppressed() {
    return g_context_menu_suppressed.load();
}

BrowserClient::BrowserClient() = default;

// CefLifeSpanHandler methods

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;

    if (on_browser_created_) {
        on_browser_created_(browser);
    }
}

bool BrowserClient::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    // Allow the close
    return false;
}

void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    // Check if browser_ is valid before comparing
    if (browser_ && browser_->IsSame(browser)) {
        browser_ = nullptr;
    }

    // Notify that browser is closing
    if (on_close_) {
        on_close_();
    }

    // Note: CefQuitMessageLoop() is called by MainWindowController::windowWillClose
    // when the main window closes, not here when individual tabs close
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   int popup_id,
                                   const CefString& target_url,
                                   const CefString& target_frame_name,
                                   cef_window_open_disposition_t target_disposition,
                                   bool user_gesture,
                                   const CefPopupFeatures& popup_features,
                                   CefWindowInfo& window_info,
                                   CefRefPtr<CefClient>& client,
                                   CefBrowserSettings& settings,
                                   CefRefPtr<CefDictionaryValue>& extra_info,
                                   bool* no_javascript_access) {
    (void)popup_id;
    (void)frame;
    (void)target_frame_name;
    (void)user_gesture;
    (void)client;
    (void)settings;
    (void)extra_info;
    (void)no_javascript_access;
    CEF_REQUIRE_UI_THREAD();

    std::string url = target_url.ToString();

    // Allow actual popup windows for OAuth/authentication flows
    // These require real popups to communicate auth results back to opener
    if (target_disposition == CEF_WOD_NEW_POPUP) {
        // Check if this looks like an OAuth/auth popup (small window or auth URL)
        bool isAuthPopup = false;

        // Check for common OAuth/authentication domains
        if (url.find("accounts.google.com") != std::string::npos ||
            url.find("login.microsoftonline.com") != std::string::npos ||
            url.find("appleid.apple.com") != std::string::npos ||
            url.find("facebook.com/login") != std::string::npos ||
            url.find("facebook.com/v") != std::string::npos ||  // Facebook OAuth
            url.find("twitter.com/oauth") != std::string::npos ||
            url.find("x.com/oauth") != std::string::npos ||
            url.find("github.com/login/oauth") != std::string::npos ||
            url.find("oauth") != std::string::npos ||
            url.find("signin") != std::string::npos ||
            url.find("auth") != std::string::npos) {
            isAuthPopup = true;
        }

        // Also check popup features - auth popups typically have specific dimensions
        if (popup_features.widthSet && popup_features.heightSet) {
            // Small windows (typical for OAuth popups) - allow them
            if (popup_features.width <= 700 && popup_features.height <= 800) {
                isAuthPopup = true;
            }
        }

        if (isAuthPopup) {
            // Ensure minimum popup dimensions so auth content isn't clipped
            int popupWidth = 500;
            int popupHeight = 700;
            if (popup_features.widthSet && popup_features.width > popupWidth) {
                popupWidth = popup_features.width;
            }
            if (popup_features.heightSet && popup_features.height > popupHeight) {
                popupHeight = popup_features.height;
            }

            // Center on the screen where the browser window is
            int screenX = 0, screenY = 0, screenW = 0, screenH = 0;
            if (on_popup_rect_) {
                on_popup_rect_(screenX, screenY, screenW, screenH);
            }
            if (screenW > 0 && screenH > 0) {
                int x = screenX + (screenW - popupWidth) / 2;
                int y = screenY + (screenH - popupHeight) / 2;
                window_info.bounds = CefRect(x, y, popupWidth, popupHeight);
            } else {
                // Fallback: let OS position it
                window_info.bounds = CefRect(0, 0, popupWidth, popupHeight);
            }
            return false;  // Don't cancel - let CEF create the popup
        }
    }

    // For regular links/popups, open as new tab
    if (!url.empty()) {
        // Check disposition to determine how to open the link
        bool background = (target_disposition == CEF_WOD_NEW_BACKGROUND_TAB);

        if (on_open_link_) {
            // Use the open link callback which supports background tabs
            on_open_link_(url, background);
        } else if (on_popup_request_) {
            // Fallback to popup request callback (always foreground)
            on_popup_request_(url);
        } else {
            // Final fallback: load in current browser
            browser->GetMainFrame()->LoadURL(target_url);
        }
    }
    return true;  // Cancel popup, we handled it by opening as tab
}

// CefLoadHandler methods

void BrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                          bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
    CEF_REQUIRE_UI_THREAD();

    if (on_loading_state_change_) {
        on_loading_state_change_(isLoading, canGoBack, canGoForward);
    }
}

void BrowserClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 TransitionType transition_type) {
    CEF_REQUIRE_UI_THREAD();
    (void)browser;
    (void)transition_type;

    // Signal real navigation start for main frame only
    if (frame->IsMain() && on_navigation_start_) {
        on_navigation_start_();
    }
}

void BrowserClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const CefString& errorText,
                                 const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();

    // Don't display error for aborted requests (user navigated away)
    if (errorCode == ERR_ABORTED) {
        return;
    }

    // Don't display error for external protocols
    if (errorCode == ERR_UNKNOWN_URL_SCHEME) {
        return;
    }

    // Show error page - escape user-controlled content to prevent XSS
    std::string escaped_url = orbfox::utils::EscapeHtml(failedUrl.ToString());
    std::string escaped_error = orbfox::utils::EscapeHtml(errorText.ToString());

    std::ostringstream ss;
    ss << "<html><head><title>Error</title>"
       << "<style>"
       << "body { font-family: -apple-system, system-ui, sans-serif; "
       << "background: #1a1a1a; color: #fff; padding: 40px; }"
       << "h1 { color: #ff6b6b; }"
       << "p { color: #888; }"
       << "code { background: #333; padding: 2px 6px; border-radius: 3px; }"
       << "</style></head><body>"
       << "<h1>Failed to load page</h1>"
       << "<p>URL: <code>" << escaped_url << "</code></p>"
       << "<p>Error: " << escaped_error << " (" << errorCode << ")</p>"
       << "</body></html>";

    // URL-encode the HTML content for the data: URL to handle special characters
    frame->LoadURL("data:text/html;charset=utf-8," + orbfox::utils::UrlEncode(ss.str()));
}

// CefDisplayHandler methods

void BrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
    CEF_REQUIRE_UI_THREAD();

    if (on_title_change_) {
        on_title_change_(title.ToString());
    }
}

void BrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const CefString& url) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain() && on_address_change_) {
        on_address_change_(url.ToString());
    }
}

void BrowserClient::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                                       const std::vector<CefString>& icon_urls) {
    CEF_REQUIRE_UI_THREAD();

    if (!on_favicon_change_ || icon_urls.empty()) {
        return;
    }

    // Use the first favicon URL (usually the best one)
    std::string favicon_url = icon_urls[0].ToString();

    // Download the favicon image
    CefRefPtr<FaviconDownloadCallback> callback =
        new FaviconDownloadCallback(favicon_url, on_favicon_change_);

    browser->GetHost()->DownloadImage(
        favicon_url,
        true,   // is_favicon
        16,     // max_image_size (16x16 for tab icons)
        false,  // bypass_cache
        callback
    );
}

void BrowserClient::OnFullscreenModeChange(CefRefPtr<CefBrowser> browser,
                                           bool fullscreen) {
    CEF_REQUIRE_UI_THREAD();

    if (on_fullscreen_change_) {
        on_fullscreen_change_(fullscreen);
    }
}

// CefRequestHandler methods

bool BrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    bool user_gesture,
                                    bool is_redirect) {
    CEF_REQUIRE_UI_THREAD();

    std::string url = request->GetURL().ToString();

    // Handle bookmark page actions
    if (url.find("orbfox://bookmarks/action/") == 0) {
        std::string action = url.substr(26);  // After "orbfox://bookmarks/action/"

        if (action == "add") {
            // Trigger add bookmark dialog
            if (on_bookmark_action_) {
                on_bookmark_action_("add", "");
            }
            return true;  // Cancel navigation
        } else if (action.find("open-new-tab?url=") == 0) {
            // Open URL in new tab
            std::string encoded_url = action.substr(17);  // After "open-new-tab?url="
            // URL decode
            std::string decoded_url;
            for (size_t i = 0; i < encoded_url.length(); ++i) {
                if (encoded_url[i] == '%' && i + 2 < encoded_url.length()) {
                    int hex = std::stoi(encoded_url.substr(i + 1, 2), nullptr, 16);
                    decoded_url += static_cast<char>(hex);
                    i += 2;
                } else if (encoded_url[i] == '+') {
                    decoded_url += ' ';
                } else {
                    decoded_url += encoded_url[i];
                }
            }
            if (on_open_link_) {
                on_open_link_(decoded_url, false);
            }
            return true;  // Cancel navigation
        } else if (action.find("open-background?url=") == 0) {
            // Open URL in background tab
            std::string encoded_url = action.substr(20);  // After "open-background?url="
            // URL decode
            std::string decoded_url;
            for (size_t i = 0; i < encoded_url.length(); ++i) {
                if (encoded_url[i] == '%' && i + 2 < encoded_url.length()) {
                    int hex = std::stoi(encoded_url.substr(i + 1, 2), nullptr, 16);
                    decoded_url += static_cast<char>(hex);
                    i += 2;
                } else if (encoded_url[i] == '+') {
                    decoded_url += ' ';
                } else {
                    decoded_url += encoded_url[i];
                }
            }
            if (on_open_link_) {
                on_open_link_(decoded_url, true);
            }
            return true;  // Cancel navigation
        } else if (action.find("edit?id=") == 0) {
            // Edit bookmark
            std::string id_str = action.substr(8);
            if (on_bookmark_action_) {
                on_bookmark_action_("edit", id_str);
            }
            return true;  // Cancel navigation
        }
        // delete action is handled by scheme handler, allow it to proceed
    }

    // Reset blocked count on main frame navigation
    if (frame->IsMain()) {
        blocked_count_ = 0;
        if (on_blocked_count_) {
            on_blocked_count_(0);
        }
    }
    return false;
}

bool BrowserClient::OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      const CefString& target_url,
                                      cef_window_open_disposition_t target_disposition,
                                      bool user_gesture) {
    CEF_REQUIRE_UI_THREAD();
    (void)browser;
    (void)frame;
    (void)user_gesture;

    if (!target_url.empty()) {
        std::string url = target_url.ToString();

        // Check disposition to determine how to open the link
        // Middle-click sends CEF_WOD_NEW_BACKGROUND_TAB
        bool background = (target_disposition == CEF_WOD_NEW_BACKGROUND_TAB);

        if (on_open_link_) {
            on_open_link_(url, background);
            return true;  // We handled it
        }
    }
    return false;  // Let CEF handle it
}

// Static list of blocked domains (ads, trackers, analytics)
const std::set<std::string>& BrowserClient::GetBlockedDomains() {
    static const std::set<std::string> domains = {
        // Google Ads & Analytics
        "googleadservices.com",
        "googlesyndication.com",
        "doubleclick.net",
        "google-analytics.com",
        "googletagmanager.com",
        "googletagservices.com",
        "pagead2.googlesyndication.com",
        "adservice.google.com",
        // Facebook
        "facebook.net",
        "connect.facebook.net",
        "pixel.facebook.com",
        "an.facebook.com",
        // Twitter/X
        "ads-twitter.com",
        "static.ads-twitter.com",
        "analytics.twitter.com",
        // Amazon
        "amazon-adsystem.com",
        "aax.amazon-adsystem.com",
        // Microsoft
        "ads.microsoft.com",
        "bat.bing.com",
        // Common ad networks
        "adnxs.com",
        "adsrvr.org",
        "advertising.com",
        "taboola.com",
        "outbrain.com",
        "criteo.com",
        "criteo.net",
        "pubmatic.com",
        "rubiconproject.com",
        "openx.net",
        "casalemedia.com",
        "sharethrough.com",
        "indexww.com",
        "33across.com",
        "media.net",
        "adform.net",
        "smartadserver.com",
        "bidswitch.net",
        // Tracking & Analytics
        "scorecardresearch.com",
        "quantserve.com",
        "segment.io",
        "segment.com",
        "mixpanel.com",
        "hotjar.com",
        "fullstory.com",
        "mouseflow.com",
        "crazyegg.com",
        "optimizely.com",
        "amplitude.com",
        "branch.io",
        "adjust.com",
        "appsflyer.com",
        "kochava.com",
        "moat.com",
        "doubleverify.com",
        "adsafeprotected.com",
        // Social widgets & tracking
        "addthis.com",
        "sharethis.com",
        "addtoany.com",
        // Other common trackers
        "newrelic.com",
        "nr-data.net",
        "bugsnag.com",
        "sentry.io",
        "rollbar.com",
        "loggly.com",
        "sumologic.com",
    };
    return domains;
}

// Helper to extract domain from URL
static std::string ExtractDomain(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) return "";
    start += 3;

    size_t end = url.find('/', start);
    if (end == std::string::npos) end = url.length();

    std::string host = url.substr(start, end - start);

    // Remove port if present
    size_t port = host.find(':');
    if (port != std::string::npos) {
        host = host.substr(0, port);
    }

    return host;
}

// Check if domain matches any blocked domain (including subdomains)
static bool IsDomainBlocked(const std::string& domain, const std::set<std::string>& blocked) {
    if (domain.empty()) return false;

    // Direct match
    if (blocked.count(domain)) return true;

    // Check if it's a subdomain of a blocked domain
    for (const auto& blockedDomain : blocked) {
        if (domain.length() > blockedDomain.length()) {
            size_t pos = domain.length() - blockedDomain.length();
            if (domain[pos - 1] == '.' &&
                domain.substr(pos) == blockedDomain) {
                return true;
            }
        }
    }
    return false;
}

CefRefPtr<CefResourceRequestHandler> BrowserClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling) {
    // Note: This method can be called on any thread (UI thread for navigations,
    // IO thread for sub-resources). No thread assertion here intentionally.
    // Return this to handle resource requests
    return this;
}

CefResourceRequestHandler::ReturnValue BrowserClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
    CEF_REQUIRE_IO_THREAD();

    // Read tracking protection setting directly from SettingsStorage (now thread-safe)
    // This ensures live updates when settings change
    if (!SettingsStorage::GetInstance().Get().tracking_protection) {
        return RV_CONTINUE;  // Tracking protection disabled, allow all
    }

    std::string url = request->GetURL().ToString();
    std::string domain = ExtractDomain(url);

    if (IsDomainBlocked(domain, GetBlockedDomains())) {
        blocked_count_++;
        // Note: blocked_count_ is atomic, so it's thread-safe to increment from IO thread.
        // The UI can query GetBlockedCount() when needed.
        // We don't call on_blocked_count_ callback here since it expects to run on UI thread.
        return RV_CANCEL;  // Block the request
    }

    return RV_CONTINUE;  // Allow the request
}

// CefContextMenuHandler methods

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefContextMenuParams> params,
                                         CefRefPtr<CefMenuModel> model) {
    CEF_REQUIRE_UI_THREAD();

    // Clear default menu
    model->Clear();

    // If context menu is suppressed (gesture tracking in progress), don't show menu
    if (IsContextMenuSuppressed()) {
        return;
    }

    // Check if we're on an internal orbfox:// page
    std::string page_url = frame->GetURL().ToString();
    bool is_internal_page = (page_url.rfind("orbfox://", 0) == 0);

    // Check if right-clicking on a link
    CefString link_url = params->GetLinkUrl();
    bool is_link = !link_url.empty();

    // Check if there's selected text
    CefString selection = params->GetSelectionText();
    bool has_selection = !selection.empty();

    // Check if right-clicking on an image
    bool is_image = (params->GetMediaType() == CM_MEDIATYPE_IMAGE);
    CefString image_url = params->GetSourceUrl();

    if (is_image && !image_url.empty()) {
        // Image context menu
        model->AddItem(MENU_ID_SAVE_IMAGE, "Save Image As...");
        model->AddItem(MENU_ID_COPY_IMAGE_ADDRESS, "Copy Image Address");
        model->AddItem(MENU_ID_OPEN_IMAGE_NEW_TAB, "Open Image in New Tab");
        model->AddSeparator();
    }

    if (is_link) {
        // Link context menu
        model->AddItem(MENU_ID_OPEN_LINK_NEW_TAB, "Open Link in New Tab");
        model->AddItem(MENU_ID_OPEN_LINK_BACKGROUND, "Open Link in Background Tab");
        model->AddSeparator();
        model->AddItem(MENU_ID_COPY_LINK_ADDRESS, "Copy Link Address");
        model->AddItem(MENU_ID_ADD_LINK_BOOKMARK, "Add Link to Bookmarks");
        model->AddSeparator();
    }

    if (has_selection) {
        // Text selection menu
        model->AddItem(MENU_ID_COPY_TEXT, "Copy");
        model->AddSeparator();
    }

    // Add navigation items
    model->AddItem(MENU_ID_BACK, "Back");
    model->AddItem(MENU_ID_FORWARD, "Forward");

    // Hide reload/stop for internal pages
    if (!is_internal_page) {
        model->AddSeparator();
        model->AddItem(MENU_ID_RELOAD, "Reload");
        model->AddItem(MENU_ID_STOP, "Stop");
    }

    // Disable items based on state
    if (!browser->CanGoBack()) {
        model->SetEnabled(MENU_ID_BACK, false);
    }
    if (!browser->CanGoForward()) {
        model->SetEnabled(MENU_ID_FORWARD, false);
    }

    // Add bookmark option for the current page (not for internal pages)
    if (!is_internal_page) {
        model->AddSeparator();
        model->AddItem(MENU_ID_BOOKMARK_PAGE, "Bookmark This Page");
    }

    // Always add Inspect at the bottom
    model->AddSeparator();
    model->AddItem(MENU_ID_INSPECT_ELEMENT, "Inspect");
}

bool BrowserClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefContextMenuParams> params,
                                          int command_id,
                                          EventFlags event_flags) {
    CEF_REQUIRE_UI_THREAD();
    (void)event_flags;

    switch (command_id) {
        case MENU_ID_BACK:
            browser->GoBack();
            return true;
        case MENU_ID_FORWARD:
            browser->GoForward();
            return true;
        case MENU_ID_RELOAD:
            browser->Reload();
            return true;
        case MENU_ID_STOP:
            browser->StopLoad();
            return true;
        case MENU_ID_OPEN_LINK_NEW_TAB: {
            CefString link_url = params->GetLinkUrl();
            if (!link_url.empty() && on_open_link_) {
                on_open_link_(link_url.ToString(), false);  // false = foreground
            }
            return true;
        }
        case MENU_ID_OPEN_LINK_BACKGROUND: {
            CefString link_url = params->GetLinkUrl();
            if (!link_url.empty() && on_open_link_) {
                on_open_link_(link_url.ToString(), true);  // true = background
            }
            return true;
        }
        case MENU_ID_COPY_LINK_ADDRESS: {
            CefString link_url = params->GetLinkUrl();
            if (!link_url.empty() && on_copy_to_clipboard_) {
                on_copy_to_clipboard_(link_url.ToString());
            }
            return true;
        }
        case MENU_ID_COPY_TEXT: {
            frame->Copy();
            return true;
        }
        case MENU_ID_INSPECT_ELEMENT: {
            if (on_inspect_element_) {
                on_inspect_element_(params->GetXCoord(), params->GetYCoord());
            }
            return true;
        }
        case MENU_ID_ADD_LINK_BOOKMARK: {
            CefString link_url = params->GetLinkUrl();
            if (!link_url.empty() && on_bookmark_action_) {
                // Use link text as title, fallback to URL
                CefString link_text = params->GetUnfilteredLinkUrl();
                std::string url = link_url.ToString();
                std::string title = url;  // Default title is URL
                on_bookmark_action_("add_link", url + "\t" + title);
            }
            return true;
        }
        case MENU_ID_BOOKMARK_PAGE: {
            if (on_bookmark_action_) {
                on_bookmark_action_("add", "");
            }
            return true;
        }
        case MENU_ID_SAVE_IMAGE: {
            CefString image_url = params->GetSourceUrl();
            if (!image_url.empty()) {
                // Trigger download of the image
                browser->GetHost()->StartDownload(image_url);
            }
            return true;
        }
        case MENU_ID_COPY_IMAGE_ADDRESS: {
            CefString image_url = params->GetSourceUrl();
            if (!image_url.empty() && on_copy_to_clipboard_) {
                on_copy_to_clipboard_(image_url.ToString());
            }
            return true;
        }
        case MENU_ID_OPEN_IMAGE_NEW_TAB: {
            CefString image_url = params->GetSourceUrl();
            if (!image_url.empty() && on_open_link_) {
                on_open_link_(image_url.ToString(), false);  // false = foreground
            }
            return true;
        }
    }
    return false;
}

// CefKeyboardHandler methods

bool BrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                   const CefKeyEvent& event,
                                   CefEventHandle os_event,
                                   bool* is_keyboard_shortcut) {
    CEF_REQUIRE_UI_THREAD();

    if (event.type == KEYEVENT_RAWKEYDOWN) {
        int vk = event.windows_key_code;
        int nk = event.native_key_code;

        // Handle Escape key in fullscreen - tell the webpage to exit fullscreen via JavaScript
        // This triggers document.exitFullscreen() which fires OnFullscreenModeChange(false)
        // ensuring both browser UI and webpage (e.g., YouTube) exit fullscreen together
        if (vk == 27 && content_fullscreen_) {  // VK_ESCAPE = 27
            CefRefPtr<CefFrame> frame = browser->GetMainFrame();
            if (frame) {
                frame->ExecuteJavaScript(
                    "if (document.fullscreenElement) { document.exitFullscreen(); }",
                    frame->GetURL(), 0);
            }
            return true;  // Consume the event, JS will handle the exit
        }

        // When in content fullscreen, prevent modifier-only key presses from reaching the page
        // This prevents accidental fullscreen exit when pressing Cmd+Shift for screenshots
        if (content_fullscreen_) {
            // Check if this is a modifier-only key press using windows_key_code (cross-platform)
            // VK_SHIFT=16, VK_CONTROL=17, VK_MENU(Alt)=18, VK_LWIN/RWIN=91/92, VK_SNAPSHOT(PrintScreen)=44
            bool isModifierOrScreenshot = (vk == 16 ||   // VK_SHIFT
                                           vk == 17 ||   // VK_CONTROL
                                           vk == 18 ||   // VK_MENU (Alt)
                                           vk == 91 ||   // VK_LWIN (Windows/Cmd key)
                                           vk == 92 ||   // VK_RWIN (Windows/Cmd key)
                                           vk == 44);    // VK_SNAPSHOT (Print Screen)

            // Also check macOS-specific native key codes for modifier keys
            // macOS: Shift=56/60, Control=59/62, Option=58/61, Command=55/54
            bool isMacModifier = (nk == 56 || nk == 60 ||  // Shift
                                  nk == 59 || nk == 62 ||  // Control
                                  nk == 58 || nk == 61 ||  // Option
                                  nk == 55 || nk == 54);   // Command

            if (isModifierOrScreenshot || isMacModifier) {
                // Consume modifier-only events in fullscreen to prevent unintended exits
                return true;
            }
        }

        // Handle keyboard shortcuts before the page sees them
        bool is_cmd = (event.modifiers & EVENTFLAG_COMMAND_DOWN) != 0;
        bool is_ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
        bool is_shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
        bool is_alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;
        bool is_modifier = is_cmd || is_ctrl;

        if (is_modifier) {
            switch (vk) {
                case 'R':  // Cmd/Ctrl+R: Reload
                    if (!is_shift) {
                        browser->Reload();
                        return true;
                    }
                    break;

                case 'L':  // Cmd/Ctrl+L: Focus URL bar
                    if (!is_shift) {
                        if (on_focus_url_bar_) {
                            on_focus_url_bar_();
                        }
                        *is_keyboard_shortcut = true;
                        return true;
                    }
                    break;

                default:
                    break;
            }

#ifdef PLATFORM_MAC
            // Cmd+[: Back, Cmd+]: Forward (macOS native key codes)
            if (nk == 33 && !is_shift) {  // '[' key on macOS
                if (browser->CanGoBack()) {
                    browser->GoBack();
                }
                return true;
            }
            if (nk == 30 && !is_shift) {  // ']' key on macOS
                if (browser->CanGoForward()) {
                    browser->GoForward();
                }
                return true;
            }
#endif

#ifdef PLATFORM_WIN
            // Windows: forward shortcuts to MainWindow via custom message
            // since BrowserClient can't perform tab/panel operations directly
            HWND browserHwnd = browser->GetHost()->GetWindowHandle();
            HWND rootHwnd = GetAncestor(browserHwnd, GA_ROOT);

            // Ctrl+[: Back, Ctrl+]: Forward
            if (vk == 0xDB && !is_shift) {  // VK_OEM_4 = '['
                if (browser->CanGoBack()) {
                    browser->GoBack();
                }
                return true;
            }
            if (vk == 0xDD && !is_shift) {  // VK_OEM_6 = ']'
                if (browser->CanGoForward()) {
                    browser->GoForward();
                }
                return true;
            }

            // Shortcuts that need MainWindow — forward via PostMessage
            if (vk == 'T' && !is_shift && !is_alt) {  // Ctrl+T: New tab
                PostMessage(rootHwnd, WM_APP + 1, 1, 0);
                return true;
            }
            if (vk == 'W' && !is_shift && !is_alt) {  // Ctrl+W: Close tab
                PostMessage(rootHwnd, WM_APP + 1, 2, 0);
                return true;
            }
            if (vk == 'F' && !is_shift && !is_alt) {  // Ctrl+F: Find
                PostMessage(rootHwnd, WM_APP + 1, 3, 0);
                return true;
            }
            if (vk == 'T' && is_shift && !is_alt) {  // Ctrl+Shift+T: Reopen tab
                PostMessage(rootHwnd, WM_APP + 1, 4, 0);
                return true;
            }
            if (vk == 'I' && is_alt) {  // Ctrl+Alt+I: Toggle DevTools
                PostMessage(rootHwnd, WM_APP + 1, 5, 0);
                return true;
            }
            if (vk == VK_LEFT && is_alt) {  // Ctrl+Alt+Left: Previous workspace
                PostMessage(rootHwnd, WM_APP + 1, 6, 0);
                return true;
            }
            if (vk == VK_RIGHT && is_alt) {  // Ctrl+Alt+Right: Next workspace
                PostMessage(rootHwnd, WM_APP + 1, 7, 0);
                return true;
            }
            // Ctrl+1-9: Switch to tab
            if (vk >= '1' && vk <= '9' && !is_shift && !is_alt) {
                PostMessage(rootHwnd, WM_APP + 1, 8, vk - '1');
                return true;
            }
#endif
        }

#ifdef PLATFORM_WIN
        // F12: Toggle DevTools (no modifier needed)
        if (vk == VK_F12) {
            HWND browserHwnd = browser->GetHost()->GetWindowHandle();
            HWND rootHwnd = GetAncestor(browserHwnd, GA_ROOT);
            PostMessage(rootHwnd, WM_APP + 1, 5, 0);
            return true;
        }

        // Escape: hide find bar
        if (vk == VK_ESCAPE && !content_fullscreen_) {
            HWND browserHwnd = browser->GetHost()->GetWindowHandle();
            HWND rootHwnd = GetAncestor(browserHwnd, GA_ROOT);
            // Forward to MainWindow — it will hide find bar if visible
            PostMessage(rootHwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            return false;  // Also let CEF process it
        }
#endif
    }
    return false;
}

// CefDownloadHandler methods

bool BrowserClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefDownloadItem> download_item,
                                     const CefString& suggested_name,
                                     CefRefPtr<CefBeforeDownloadCallback> callback) {
    (void)browser;
    CEF_REQUIRE_UI_THREAD();

    // If we have a dialog callback, show the download dialog
    if (on_download_dialog_) {
        on_download_dialog_(
            suggested_name.ToString(),
            download_item->GetTotalBytes(),
            callback
        );
        return true;
    }

    // Fallback: direct download to ~/Downloads
    // Sanitize filename to prevent path traversal attacks
    std::string filename = suggested_name.ToString();

    // Remove any path components (keep only the filename)
    size_t last_slash = filename.rfind('/');
    if (last_slash != std::string::npos) {
        filename = filename.substr(last_slash + 1);
    }
    size_t last_backslash = filename.rfind('\\');
    if (last_backslash != std::string::npos) {
        filename = filename.substr(last_backslash + 1);
    }

    // Remove any remaining .. sequences
    while (filename.find("..") != std::string::npos) {
        size_t pos = filename.find("..");
        filename.erase(pos, 2);
    }

    // If filename is empty after sanitization, use a default
    if (filename.empty()) {
        filename = "download";
    }

#ifdef PLATFORM_WIN
    std::string downloads_path = orbfox::utils::GetHomeDirectory() + "\\Downloads\\" + filename;
#else
    std::string downloads_path = orbfox::utils::GetHomeDirectory() + "/Downloads/" + filename;
#endif

    callback->Continue(downloads_path, false);
    return true;
}

void BrowserClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefDownloadItem> download_item,
                                      CefRefPtr<CefDownloadItemCallback> callback) {
    (void)browser;
    CEF_REQUIRE_UI_THREAD();

    uint32_t download_id = download_item->GetId();
    std::string full_path = download_item->GetFullPath().ToString();

    // Only track downloads that have been confirmed (have a save path)
    // This prevents showing downloads before user clicks Save/Save As
    if (full_path.empty()) {
        // Download not yet confirmed or was canceled before starting
        if (download_item->IsCanceled()) {
            download_callbacks_.erase(download_id);
        }
        return;
    }

    // Check if this is a new download (first time we see it with valid path)
    bool is_new_download = (download_callbacks_.find(download_id) == download_callbacks_.end());

    // Store callback for later use (cancel/pause/resume)
    download_callbacks_[download_id] = callback;

    // Register cancel callback with DownloadManager
    CefRefPtr<CefDownloadItemCallback> cancel_cb = callback;
    DownloadManager::GetInstance().SetCancelCallback(download_id, [cancel_cb]() {
        if (cancel_cb) {
            cancel_cb->Cancel();
        }
    });

    // Add to history when download starts (first time only)
    if (is_new_download) {
        std::string url = download_item->GetURL().ToString();
        if (!url.empty()) {
            HistoryStorage* history = GetHistoryStorage();
            if (history) {
                // Extract filename for title
                std::string filename;
                size_t lastSlash = full_path.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    filename = full_path.substr(lastSlash + 1);
                } else {
                    filename = full_path;
                }
                std::string title = "Download: " + filename;
                history->AddEntry(url, title);
            }
        }
    }

    // Update download item
    DownloadItem item;
    item.id = download_id;
    item.url = download_item->GetURL().ToString();

    // Set original URL - check for pending URL first (for restarts), then use CEF's URL
    if (is_new_download) {
        std::string pending_url = DownloadManager::GetInstance().GetAndClearPendingOriginalUrl();
        if (!pending_url.empty()) {
            item.original_url = pending_url;
        } else {
            // For new downloads triggered by clicking a link, use the original URL from CEF if available
            std::string cef_original = download_item->GetOriginalUrl().ToString();
            if (!cef_original.empty()) {
                item.original_url = cef_original;
            } else {
                item.original_url = item.url;
            }
        }
    }

    item.full_path = full_path;
    item.mime_type = download_item->GetMimeType().ToString();
    item.total_bytes = download_item->GetTotalBytes();
    item.received_bytes = download_item->GetReceivedBytes();
    item.percent_complete = download_item->GetPercentComplete();
    item.current_speed = download_item->GetCurrentSpeed();

    // Get filename - extract from path (most reliable)
    size_t lastSlash = full_path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        item.filename = full_path.substr(lastSlash + 1);
    } else {
        item.filename = full_path;
    }

    // Determine state
    bool should_save = false;
    if (download_item->IsComplete()) {
        item.state = DownloadState::Complete;
        item.end_time = std::time(nullptr);
        download_callbacks_.erase(download_id);  // No longer need callback
        should_save = true;
    } else if (download_item->IsCanceled()) {
        item.state = DownloadState::Canceled;
        item.end_time = std::time(nullptr);
        download_callbacks_.erase(download_id);
        should_save = true;
    } else if (download_item->IsInterrupted()) {
        item.state = DownloadState::Interrupted;
        download_callbacks_.erase(download_id);  // Clean up callback for interrupted downloads
        should_save = true;
    } else if (download_item->IsPaused()) {
        item.state = DownloadState::Paused;
    } else if (download_item->IsInProgress()) {
        item.state = DownloadState::InProgress;
    }

    DownloadManager::GetInstance().UpdateDownload(item);

    // Save to disk when download finishes (complete, canceled, or interrupted)
    if (should_save) {
        DownloadManager::GetInstance().SaveToDisk();
    }
}

// CefFindHandler implementation
void BrowserClient::OnFindResult(CefRefPtr<CefBrowser> browser,
                                 int identifier,
                                 int count,
                                 const CefRect& selectionRect,
                                 int activeMatchOrdinal,
                                 bool finalUpdate) {
    CEF_REQUIRE_UI_THREAD();
    (void)browser;
    (void)identifier;
    (void)selectionRect;
    (void)finalUpdate;

    if (on_find_result_) {
        on_find_result_(count, activeMatchOrdinal);
    }
}
