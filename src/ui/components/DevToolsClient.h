#pragma once

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

// Minimal CefClient for DevTools panel
class DevToolsClient : public CefClient,
                       public CefLifeSpanHandler,
                       public CefLoadHandler {
public:
    DevToolsClient() = default;

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

    IMPLEMENT_REFCOUNTING(DevToolsClient);
    DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};
