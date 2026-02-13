#pragma once

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

#include <functional>

// Minimal CefClient for DevTools panel
class DevToolsClient : public CefClient,
                       public CefLifeSpanHandler,
                       public CefLoadHandler {
public:
    using CloseCallback = std::function<void()>;

    DevToolsClient() = default;

    void SetCloseCallback(CloseCallback callback) { on_close_ = std::move(callback); }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        (void)browser;
        browser_ = nullptr;
        if (on_close_) {
            on_close_();
        }
    }

    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override {
        (void)httpStatusCode;
        (void)browser;
        if (!frame->IsMain()) return;
        // DevTools loaded - no JavaScript injection needed
    }

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

private:
    CefRefPtr<CefBrowser> browser_;
    CloseCallback on_close_;

    IMPLEMENT_REFCOUNTING(DevToolsClient);
    DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};
