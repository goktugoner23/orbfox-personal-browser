#include "browser_client.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_image.h"
#include "include/wrapper/cef_helpers.h"

#include <sstream>

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

    // Show error page
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
       << "<p>URL: <code>" << failedUrl.ToString() << "</code></p>"
       << "<p>Error: " << errorText.ToString() << " (" << errorCode << ")</p>"
       << "</body></html>";

    frame->LoadURL("data:text/html;charset=utf-8," + ss.str());
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

// CefRequestHandler methods

bool BrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    bool user_gesture,
                                    bool is_redirect) {
    CEF_REQUIRE_UI_THREAD();
    // Allow all navigation for now
    return false;
}

// CefContextMenuHandler methods

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefContextMenuParams> params,
                                         CefRefPtr<CefMenuModel> model) {
    CEF_REQUIRE_UI_THREAD();

    // Clear default menu
    model->Clear();

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
}

bool BrowserClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefContextMenuParams> params,
                                          int command_id,
                                          EventFlags event_flags) {
    CEF_REQUIRE_UI_THREAD();

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
    }
    return false;
}

// CefKeyboardHandler methods

bool BrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                   const CefKeyEvent& event,
                                   CefEventHandle os_event,
                                   bool* is_keyboard_shortcut) {
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
                    // TODO: Signal to focus URL bar
                    *is_keyboard_shortcut = true;
                    return false;

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
