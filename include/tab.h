#pragma once

#include <string>
#include <vector>

// CEF includes - only when not in unit test mode
#ifndef UNIT_TEST
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "browser_client.h"
#endif

// Forward declarations for unit tests
#ifndef UNIT_TEST
class BrowserClient;
#endif

// Represents a single browser tab
struct Tab {
    int id = 0;
    std::string url;
    std::string title = "New Tab";
    std::string favicon_url;
    std::vector<unsigned char> favicon_data;  // PNG data
    bool is_loading = false;
    bool is_pinned = false;
    bool is_muted = false;
    bool is_hibernated = false;  // True if browser is suspended to save memory
    int64_t last_active_time = 0;  // Timestamp when tab was last active (for auto-hibernate)

#ifndef UNIT_TEST
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<BrowserClient> client;
#endif

    Tab() = default;
    explicit Tab(int tab_id) : id(tab_id) {}
};
