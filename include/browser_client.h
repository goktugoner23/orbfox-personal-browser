#pragma once

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_find_handler.h"

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <atomic>

// Global context menu suppression for gesture detection
// Call these from the gesture container to temporarily disable context menus
void SuppressContextMenu(bool suppress);
bool IsContextMenuSuppressed();

// BrowserClient: Per-browser CEF callbacks
// Handles all browser-level events (lifecycle, loading, display, etc.)
class BrowserClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler,
                      public CefRequestHandler,
                      public CefResourceRequestHandler,
                      public CefContextMenuHandler,
                      public CefKeyboardHandler,
                      public CefDownloadHandler,
                      public CefFindHandler {
public:
    // Callback types for UI updates
    using BrowserCreatedCallback = std::function<void(CefRefPtr<CefBrowser>)>;
    using TitleChangeCallback = std::function<void(const std::string&)>;
    using AddressChangeCallback = std::function<void(const std::string&)>;
    using LoadingStateCallback = std::function<void(bool isLoading, bool canGoBack, bool canGoForward)>;
    using NavigationStartCallback = std::function<void()>;  // Called on real navigation start
    using CloseCallback = std::function<void()>;
    using PopupRequestCallback = std::function<void(const std::string& url)>;
    using FaviconChangeCallback = std::function<void(const std::string& url, const std::vector<unsigned char>& png_data)>;
    using BlockedCountCallback = std::function<void(int blockedCount)>;
    using FullscreenChangeCallback = std::function<void(bool fullscreen)>;
    using FindResultCallback = std::function<void(int count, int activeMatch)>;
    using OpenLinkCallback = std::function<void(const std::string& url, bool background)>;
    using CopyToClipboardCallback = std::function<void(const std::string& text)>;
    using InspectElementCallback = std::function<void(int x, int y)>;
    using FocusUrlBarCallback = std::function<void()>;
    using BookmarkActionCallback = std::function<void(const std::string& action, const std::string& param)>;
    // Returns screen rect (x, y, width, height) for centering popups on the correct monitor
    using PopupRectCallback = std::function<void(int& x, int& y, int& width, int& height)>;

    // Download dialog callback: filename, size, callback to continue with path (empty = cancel)
    using DownloadDialogCallback = std::function<void(
        const std::string& suggested_name,
        int64_t total_bytes,
        CefRefPtr<CefBeforeDownloadCallback> callback)>;

    // Custom menu command IDs (use high values to avoid collision with CEF's built-in IDs)
    enum MenuCommands {
        MENU_ID_OPEN_LINK_NEW_TAB = 50000,
        MENU_ID_OPEN_LINK_BACKGROUND = 50001,
        MENU_ID_COPY_LINK_ADDRESS = 50002,
        MENU_ID_COPY_TEXT = 50003,
        MENU_ID_INSPECT_ELEMENT = 50004,
        MENU_ID_ADD_LINK_BOOKMARK = 50005,
        MENU_ID_BOOKMARK_PAGE = 50006,
        MENU_ID_SAVE_IMAGE = 50007,
        MENU_ID_COPY_IMAGE_ADDRESS = 50008,
        MENU_ID_OPEN_IMAGE_NEW_TAB = 50009,
    };

    BrowserClient();

    // Set callbacks for UI updates
    void SetBrowserCreatedCallback(BrowserCreatedCallback callback) { on_browser_created_ = std::move(callback); }
    void SetTitleChangeCallback(TitleChangeCallback callback) { on_title_change_ = std::move(callback); }
    void SetAddressChangeCallback(AddressChangeCallback callback) { on_address_change_ = std::move(callback); }
    void SetLoadingStateCallback(LoadingStateCallback callback) { on_loading_state_change_ = std::move(callback); }
    void SetNavigationStartCallback(NavigationStartCallback callback) { on_navigation_start_ = std::move(callback); }
    void SetCloseCallback(CloseCallback callback) { on_close_ = std::move(callback); }
    void SetPopupRequestCallback(PopupRequestCallback callback) { on_popup_request_ = std::move(callback); }
    void SetFaviconChangeCallback(FaviconChangeCallback callback) { on_favicon_change_ = std::move(callback); }
    void SetDownloadDialogCallback(DownloadDialogCallback callback) { on_download_dialog_ = std::move(callback); }
    void SetBlockedCountCallback(BlockedCountCallback callback) { on_blocked_count_ = std::move(callback); }
    void SetFullscreenChangeCallback(FullscreenChangeCallback callback) { on_fullscreen_change_ = std::move(callback); }
    void SetFindResultCallback(FindResultCallback callback) { on_find_result_ = std::move(callback); }
    void SetOpenLinkCallback(OpenLinkCallback callback) { on_open_link_ = std::move(callback); }
    void SetCopyToClipboardCallback(CopyToClipboardCallback callback) { on_copy_to_clipboard_ = std::move(callback); }
    void SetInspectElementCallback(InspectElementCallback callback) { on_inspect_element_ = std::move(callback); }
    void SetFocusUrlBarCallback(FocusUrlBarCallback callback) { on_focus_url_bar_ = std::move(callback); }
    void SetBookmarkActionCallback(BookmarkActionCallback callback) { on_bookmark_action_ = std::move(callback); }
    void SetPopupRectCallback(PopupRectCallback callback) { on_popup_rect_ = std::move(callback); }

    // Clear all UI callbacks (used during hibernation to prevent callbacks from firing
    // on a browser that is in the process of being closed)
    void ClearCallbacks();

    // Track content fullscreen state (to filter keyboard events properly)
    void SetContentFullscreen(bool fullscreen) { content_fullscreen_ = fullscreen; }
    bool IsContentFullscreen() const { return content_fullscreen_; }

    // Get blocked request count for this browser
    int GetBlockedCount() const { return blocked_count_; }

    // CefClient methods - return handler references
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
    CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                       bool* no_javascript_access) override;

    // CefLoadHandler methods
    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                              bool isLoading,
                              bool canGoBack,
                              bool canGoForward) override;
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
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
    void OnFullscreenModeChange(CefRefPtr<CefBrowser> browser,
                                bool fullscreen) override;

    // CefRequestHandler methods
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override;

    bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          const CefString& target_url,
                          cef_window_open_disposition_t target_disposition,
                          bool user_gesture) override;

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override;

    // CefResourceRequestHandler methods
    CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override;

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

    // CefDownloadHandler methods
    bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefDownloadItem> download_item,
                          const CefString& suggested_name,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override;
    void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefDownloadItem> download_item,
                           CefRefPtr<CefDownloadItemCallback> callback) override;

    // CefFindHandler methods
    void OnFindResult(CefRefPtr<CefBrowser> browser,
                      int identifier,
                      int count,
                      const CefRect& selectionRect,
                      int activeMatchOrdinal,
                      bool finalUpdate) override;

    // Get the browser instance
    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

private:
    CefRefPtr<CefBrowser> browser_;

    // UI callbacks
    BrowserCreatedCallback on_browser_created_;
    TitleChangeCallback on_title_change_;
    AddressChangeCallback on_address_change_;
    LoadingStateCallback on_loading_state_change_;
    NavigationStartCallback on_navigation_start_;
    CloseCallback on_close_;
    PopupRequestCallback on_popup_request_;
    FaviconChangeCallback on_favicon_change_;
    DownloadDialogCallback on_download_dialog_;
    BlockedCountCallback on_blocked_count_;
    FullscreenChangeCallback on_fullscreen_change_;
    FindResultCallback on_find_result_;
    OpenLinkCallback on_open_link_;
    CopyToClipboardCallback on_copy_to_clipboard_;
    InspectElementCallback on_inspect_element_;
    FocusUrlBarCallback on_focus_url_bar_;
    BookmarkActionCallback on_bookmark_action_;
    PopupRectCallback on_popup_rect_;

    // Download callbacks (keyed by download ID)
    std::map<uint32_t, CefRefPtr<CefDownloadItemCallback>> download_callbacks_;

    // Tracking/ad blocking
    std::atomic<int> blocked_count_{0};
    std::atomic<bool> content_fullscreen_{false};  // Track content fullscreen state
    static const std::set<std::string>& GetBlockedDomains();

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
