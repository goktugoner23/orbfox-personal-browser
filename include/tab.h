#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "browser_client.h"

#include <string>

// Represents a single browser tab
struct Tab {
    int id = 0;
    std::string url;
    std::string title = "New Tab";
    bool is_loading = false;
    bool is_pinned = false;
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<BrowserClient> client;

    Tab() = default;
    explicit Tab(int tab_id) : id(tab_id) {}
};
