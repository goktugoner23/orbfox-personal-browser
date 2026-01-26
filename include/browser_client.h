#pragma once

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_keyboard_handler.h"

#include <functional>
#include <string>
#include <vector>

// BrowserClient: Per-browser CEF callbacks
// Handles all browser-level events (lifecycle, loading, display, etc.)
class BrowserClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler,
                      public CefRequestHandler,
                      public CefContextMenuHandler,
                      public CefKeyboardHandler {
public:
    // Callback types for UI updates
    using BrowserCreatedCallback = std::function<void(CefRefPtr<CefBrowser>)>;
    using TitleChangeCallback = std::function<void(const std::string&)>;
    using AddressChangeCallback = std::function<void(const std::string&)>;
    using LoadingStateCallback = std::function<void(bool isLoading, bool canGoBack, bool canGoForward)>;
    using CloseCallback = std::function<void()>;
    using PopupRequestCallback = std::function<void(const std::string& url)>;
    using FaviconChangeCallback = std::function<void(const std::string& url, const std::vector<unsigned char>& png_data)>;

    BrowserClient();

    // Set callbacks for UI updates
    void SetBrowserCreatedCallback(BrowserCreatedCallback callback) { on_browser_created_ = std::move(callback); }
    void SetTitleChangeCallback(TitleChangeCallback callback) { on_title_change_ = std::move(callback); }
    void SetAddressChangeCallback(AddressChangeCallback callback) { on_address_change_ = std::move(callback); }
    void SetLoadingStateCallback(LoadingStateCallback callback) { on_loading_state_change_ = std::move(callback); }
    void SetCloseCallback(CloseCallback callback) { on_close_ = std::move(callback); }
    void SetPopupRequestCallback(PopupRequestCallback callback) { on_popup_request_ = std::move(callback); }
    void SetFaviconChangeCallback(FaviconChangeCallback callback) { on_favicon_change_ = std::move(callback); }

    // CefClient methods - return handler references
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                       bool* no_javascript_access) override;

    // CefLoadHandler methods
    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                              bool isLoading,
                              bool canGoBack,
                              bool canGoForward) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override;

    // CefDisplayHandler methods
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override;
    void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                            const std::vector<CefString>& icon_urls) override;

    // CefRequestHandler methods
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override;

    // CefContextMenuHandler methods
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override;
    bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefContextMenuParams> params,
                              int command_id,
                              EventFlags event_flags) override;

    // CefKeyboardHandler methods
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override;

    // Get the browser instance
    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

private:
    CefRefPtr<CefBrowser> browser_;

    // UI callbacks
    BrowserCreatedCallback on_browser_created_;
    TitleChangeCallback on_title_change_;
    AddressChangeCallback on_address_change_;
    LoadingStateCallback on_loading_state_change_;
    CloseCallback on_close_;
    PopupRequestCallback on_popup_request_;
    FaviconChangeCallback on_favicon_change_;

    // Context menu command IDs
    enum MenuCommand {
        MENU_ID_BACK = MENU_ID_USER_FIRST,
        MENU_ID_FORWARD,
        MENU_ID_RELOAD,
        MENU_ID_STOP,
    };

    IMPLEMENT_REFCOUNTING(BrowserClient);
    DISALLOW_COPY_AND_ASSIGN(BrowserClient);
};
