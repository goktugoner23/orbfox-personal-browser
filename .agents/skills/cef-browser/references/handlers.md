# CEF Handler Reference

## CefLifeSpanHandler

Controls browser lifecycle events.

```cpp
class MyLifeSpanHandler : public CefLifeSpanHandler {
public:
    // Called after a new browser is created
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        // Store browser reference
        browsers_.push_back(browser);
    }

    // Called when a browser has received a close request
    // Return true to cancel the close, false to allow
    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        // For main window, may want to prevent accidental close
        if (is_main_browser_) {
            // Show confirmation dialog, etc.
            return false;  // Allow close
        }
        return false;
    }

    // Called just before a browser is destroyed
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        // Remove from list
        auto it = std::find_if(browsers_.begin(), browsers_.end(),
            [&](const CefRefPtr<CefBrowser>& b) {
                return b->GetIdentifier() == browser->GetIdentifier();
            });
        if (it != browsers_.end()) {
            browsers_.erase(it);
        }

        // Quit message loop when all browsers close
        if (browsers_.empty()) {
            CefQuitMessageLoop();
        }
    }

    // Called before a popup is created
    // Return true to cancel popup, false to allow
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& target_url,
                       const CefString& target_frame_name,
                       WindowOpenDisposition target_disposition,
                       bool user_gesture,
                       const CefPopupFeatures& popupFeatures,
                       CefWindowInfo& windowInfo,
                       CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info,
                       bool* no_javascript_access) override {
        // Load popup URL in current browser instead
        browser->GetMainFrame()->LoadURL(target_url);
        return true;  // Cancel popup
    }

    IMPLEMENT_REFCOUNTING(MyLifeSpanHandler);

private:
    std::vector<CefRefPtr<CefBrowser>> browsers_;
    bool is_main_browser_ = true;
};
```

## CefLoadHandler

Handles page loading events.

```cpp
class MyLoadHandler : public CefLoadHandler {
public:
    static std::string HtmlEscape(std::string value) {
        std::string escaped;
        escaped.reserve(value.size());

        for (char ch : value) {
            switch (ch) {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                case '"': escaped += "&quot;"; break;
                case '\'': escaped += "&#39;"; break;
                default: escaped += ch; break;
            }
        }

        return escaped;
    }

    // Called when loading state changes
    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                              bool isLoading,
                              bool canGoBack,
                              bool canGoForward) override {
        // Update navigation buttons
        if (on_nav_state_change_) {
            on_nav_state_change_(canGoBack, canGoForward);
        }

        // Update loading indicator
        if (on_loading_change_) {
            on_loading_change_(isLoading);
        }
    }

    // Called when a frame starts loading
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override {
        if (frame->IsMain()) {
            // Main frame started loading
        }
    }

    // Called when a frame finishes loading
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override {
        if (frame->IsMain()) {
            // Main frame finished loading
            // httpStatusCode is the HTTP status (200, 404, etc.)
        }
    }

    // Called when a load fails
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override {
        // Don't display error for aborted loads (user navigation)
        if (errorCode == ERR_ABORTED) {
            return;
        }

        // Show error page with escaped content and an encoded data URL.
        const std::string html = "<html><body><h2>Failed to load</h2>"
                                 "<p>URL: " + HtmlEscape(failedUrl.ToString()) + "</p>"
                                 "<p>Error: " + HtmlEscape(errorText.ToString()) + "</p>"
                                 "</body></html>";
        frame->LoadURL("data:text/html;charset=utf-8," +
                       CefURIEncode(html, false).ToString());
    }

    IMPLEMENT_REFCOUNTING(MyLoadHandler);

private:
    std::function<void(bool, bool)> on_nav_state_change_;
    std::function<void(bool)> on_loading_change_;
};
```

## CefDisplayHandler

Handles display-related events.

```cpp
class MyDisplayHandler : public CefDisplayHandler {
public:
    // Called when the page title changes
    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override {
        // Update window title
        if (on_title_change_) {
            on_title_change_(title.ToString());
        }
    }

    // Called when the page address changes
    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override {
        if (frame->IsMain()) {
            // Update address bar
            if (on_address_change_) {
                on_address_change_(url.ToString());
            }
        }
    }

    // Called when the favicon URL changes
    void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                            const std::vector<CefString>& icon_urls) override {
        if (!icon_urls.empty()) {
            // Download and display favicon
        }
    }

    // Called when the fullscreen state changes
    void OnFullscreenModeChange(CefRefPtr<CefBrowser> browser,
                                bool fullscreen) override {
        // Toggle window fullscreen
    }

    // Called when a console message is logged
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override {
        // Log to application console
        // Return true to stop propagation
        return false;
    }

    IMPLEMENT_REFCOUNTING(MyDisplayHandler);

private:
    std::function<void(const std::string&)> on_title_change_;
    std::function<void(const std::string&)> on_address_change_;
};
```

## CefRequestHandler

Handles request-related events (navigation, resources).

```cpp
class MyRequestHandler : public CefRequestHandler {
public:
    static bool IsBlockedHost(const CefString& request_url) {
        CefURLParts parts;
        if (!CefParseURL(request_url, parts)) {
            return false;
        }

        std::string host = CefString(&parts.host).ToString();
        std::transform(host.begin(), host.end(), host.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });

        constexpr std::string_view blocked_host = "blocked-domain.com";
        constexpr std::string_view blocked_subdomain_suffix = ".blocked-domain.com";
        return host == blocked_host ||
               (host.size() > blocked_subdomain_suffix.size() &&
                host.compare(host.size() - blocked_subdomain_suffix.size(),
                             blocked_subdomain_suffix.size(),
                             blocked_subdomain_suffix) == 0);
    }

    // Called before browser navigation
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override {
        // Block exact host and subdomains only.
        if (IsBlockedHost(request->GetURL())) {
            return true;  // Cancel navigation
        }

        return false;  // Allow navigation
    }

    IMPLEMENT_REFCOUNTING(MyRequestHandler);
};
```

## CefContextMenuHandler

Handles context menu (right-click) events.

```cpp
class MyContextMenuHandler : public CefContextMenuHandler {
public:
    // Called before context menu is displayed
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override {
        // Clear default menu
        model->Clear();

        // Add custom items
        model->AddItem(MENU_ID_BACK, "Back");
        model->AddItem(MENU_ID_FORWARD, "Forward");
        model->AddSeparator();
        model->AddItem(MENU_ID_RELOAD, "Reload");

        // Disable back if can't go back
        if (!browser->CanGoBack()) {
            model->SetEnabled(MENU_ID_BACK, false);
        }
    }

    // Called when a context menu command is selected
    bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefContextMenuParams> params,
                              int command_id,
                              EventFlags event_flags) override {
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
        }
        return false;
    }

    IMPLEMENT_REFCOUNTING(MyContextMenuHandler);

private:
    enum MenuIds {
        MENU_ID_BACK = MENU_ID_USER_FIRST,
        MENU_ID_FORWARD,
        MENU_ID_RELOAD,
    };
};
```

## CefKeyboardHandler

Handles keyboard events.

```cpp
class MyKeyboardHandler : public CefKeyboardHandler {
public:
    // Called before a keyboard event is sent to the renderer
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override {
        // Handle app-level shortcuts before the page
        if (event.type == KEYEVENT_RAWKEYDOWN) {
            // Cmd+L (macOS) or Ctrl+L (Windows) - focus address bar
            if ((event.modifiers & EVENTFLAG_COMMAND_DOWN ||
                 event.modifiers & EVENTFLAG_CONTROL_DOWN) &&
                event.windows_key_code == 'L') {
                // Focus address bar
                *is_keyboard_shortcut = true;
                return true;
            }
        }
        return false;
    }

    IMPLEMENT_REFCOUNTING(MyKeyboardHandler);
};
```
