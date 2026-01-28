#ifndef ORBFOX_SCHEME_HANDLER_H_
#define ORBFOX_SCHEME_HANDLER_H_

#include "include/cef_scheme.h"
#include "include/cef_resource_handler.h"

// Factory class for creating orbfox:// scheme handlers
class OrbfoxSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
    OrbfoxSchemeHandlerFactory() = default;

    CefRefPtr<CefResourceHandler> Create(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& scheme_name,
        CefRefPtr<CefRequest> request) override;

    IMPLEMENT_REFCOUNTING(OrbfoxSchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN(OrbfoxSchemeHandlerFactory);
};

// Resource handler for serving orbfox:// content
class OrbfoxResourceHandler : public CefResourceHandler {
public:
    OrbfoxResourceHandler();

    bool Open(CefRefPtr<CefRequest> request,
              bool& handle_request,
              CefRefPtr<CefCallback> callback) override;

    void GetResponseHeaders(CefRefPtr<CefResponse> response,
                            int64_t& response_length,
                            CefString& redirectUrl) override;

    bool Read(void* data_out,
              int bytes_to_read,
              int& bytes_read,
              CefRefPtr<CefResourceReadCallback> callback) override;

    void Cancel() override {}

private:
    std::string data_;
    std::string mime_type_;
    size_t offset_ = 0;
    int status_code_ = 200;

    void HandleSettingsPage();
    void HandleSettingsApiGet();
    void HandleSettingsApiSet(const std::string& post_data);
    void HandleNotFound(const std::string& path);

    IMPLEMENT_REFCOUNTING(OrbfoxResourceHandler);
    DISALLOW_COPY_AND_ASSIGN(OrbfoxResourceHandler);
};

// Register the orbfox:// scheme handler
void RegisterOrbfoxSchemeHandler();

#endif  // ORBFOX_SCHEME_HANDLER_H_
