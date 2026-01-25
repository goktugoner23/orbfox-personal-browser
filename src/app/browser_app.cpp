#include "browser_app.h"
#include "browser_client.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"

namespace {

// BrowserView delegate for runtime style
class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
public:
    SimpleBrowserViewDelegate() = default;

    cef_runtime_style_t GetBrowserRuntimeStyle() override {
        return CEF_RUNTIME_STYLE_DEFAULT;
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
    DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

// Window delegate for runtime style
class SimpleWindowDelegate : public CefWindowDelegate {
public:
    explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
        : browser_view_(browser_view) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        // Add the browser view and show the window
        window->AddChildView(browser_view_);
        window->Show();

        // Give keyboard focus to the browser view
        browser_view_->RequestFocus();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
        browser_view_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow> window) override {
        // Allow the window to close if the browser says it's OK
        CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
        if (browser) {
            return browser->GetHost()->TryCloseBrowser();
        }
        return true;
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
        return CefSize(1280, 800);
    }

    cef_runtime_style_t GetWindowRuntimeStyle() override {
        return CEF_RUNTIME_STYLE_DEFAULT;
    }

private:
    CefRefPtr<CefBrowserView> browser_view_;

    IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

}  // namespace

BrowserApp::BrowserApp() = default;

void BrowserApp::OnBeforeCommandLineProcessing(
    const CefString& /*process_type*/,
    CefRefPtr<CefCommandLine> /*command_line*/) {
    // Multi-process mode is now enabled via helper apps
    // No special flags needed for normal operation
}

void BrowserApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

    // Browser settings
    CefBrowserSettings browser_settings;

    // Create browser client
    CefRefPtr<BrowserClient> client(new BrowserClient());

    // Initial URL
    const std::string url = "https://www.google.com";

    // Create the BrowserView using Views framework
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client, url, browser_settings, nullptr, nullptr,
        new SimpleBrowserViewDelegate());

    // Create the Window using Views framework
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(browser_view));
}
