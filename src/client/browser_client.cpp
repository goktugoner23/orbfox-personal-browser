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
                                   WindowOpenDisposition target_disposition,
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
    (void)target_disposition;
    (void)user_gesture;
    (void)popup_features;
    (void)window_info;
    (void)client;
    (void)settings;
    (void)extra_info;
    (void)no_javascript_access;
    CEF_REQUIRE_UI_THREAD();

    // Open popup as new tab
    if (!target_url.empty()) {
        if (on_popup_request_) {
            on_popup_request_(target_url.ToString());
        } else {
            // Fallback: load in current browser
            browser->GetMainFrame()->LoadURL(target_url);
        }
    }
    return true;  // Cancel popup, we handled it
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
    // Reset blocked count on main frame navigation
    if (frame->IsMain()) {
        blocked_count_ = 0;
        if (on_blocked_count_) {
            on_blocked_count_(0);
        }
    }
    return false;
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
    (void)frame;

    // Clear default menu
    model->Clear();

    // Check if right-clicking on a link
    CefString link_url = params->GetLinkUrl();
    bool is_link = !link_url.empty();

    // Check if there's selected text
    CefString selection = params->GetSelectionText();
    bool has_selection = !selection.empty();

    if (is_link) {
        // Link context menu
        model->AddItem(MENU_ID_OPEN_LINK_NEW_TAB, "Open Link in New Tab");
        model->AddItem(MENU_ID_OPEN_LINK_BACKGROUND, "Open Link in Background Tab");
        model->AddSeparator();
        model->AddItem(MENU_ID_COPY_LINK_ADDRESS, "Copy Link Address");
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
    model->AddSeparator();
    model->AddItem(MENU_ID_RELOAD, "Reload");
    model->AddItem(MENU_ID_STOP, "Stop");

    // Disable items based on state
    if (!browser->CanGoBack()) {
        model->SetEnabled(MENU_ID_BACK, false);
    }
    if (!browser->CanGoForward()) {
        model->SetEnabled(MENU_ID_FORWARD, false);
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
    }
    return false;
}

// CefKeyboardHandler methods

bool BrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                   const CefKeyEvent& event,
                                   CefEventHandle os_event,
                                   bool* is_keyboard_shortcut) {
    CEF_REQUIRE_UI_THREAD();
    // Handle keyboard shortcuts before the page sees them
    if (event.type == KEYEVENT_RAWKEYDOWN) {
        bool is_cmd = (event.modifiers & EVENTFLAG_COMMAND_DOWN) != 0;
        bool is_ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
        bool is_modifier = is_cmd || is_ctrl;

        if (is_modifier) {
            switch (event.windows_key_code) {
                case 'R':  // Cmd+R: Reload
                    browser->Reload();
                    return true;

                case 'L':  // Cmd+L: Focus URL bar
                    if (on_focus_url_bar_) {
                        on_focus_url_bar_();
                    }
                    *is_keyboard_shortcut = true;
                    return true;

                case '[':  // Cmd+[: Back (macOS)
                    if (browser->CanGoBack()) {
                        browser->GoBack();
                    }
                    return true;

                case ']':  // Cmd+]: Forward (macOS)
                    if (browser->CanGoForward()) {
                        browser->GoForward();
                    }
                    return true;
            }
        }
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

    std::string downloads_path = orbfox::utils::GetHomeDirectory() + "/Downloads/" + filename;

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
    size_t lastSlash = full_path.find_last_of('/');
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
