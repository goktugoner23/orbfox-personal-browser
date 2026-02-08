// Helper process entry point for Windows
// Each sub-process (GPU, Renderer, etc.) runs this code
// On Windows, we use a single executable for all processes, but this file
// can be used if we want a separate helper executable like on macOS.

#ifdef PLATFORM_WIN

#include <windows.h>

#include "include/cef_app.h"
#include "include/cef_scheme.h"

// Minimal CefApp for helper processes - just registers custom schemes
class HelperApp : public CefApp {
public:
    HelperApp() = default;

    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
        // Register "orbfox" scheme in all processes
        registrar->AddCustomScheme(
            "orbfox",
            CEF_SCHEME_OPTION_STANDARD |
            CEF_SCHEME_OPTION_SECURE |
            CEF_SCHEME_OPTION_CORS_ENABLED |
            CEF_SCHEME_OPTION_FETCH_ENABLED
        );
    }

private:
    IMPLEMENT_REFCOUNTING(HelperApp);
    DISALLOW_COPY_AND_ASSIGN(HelperApp);
};

// Entry point for CEF sub-processes (if using separate helper executable)
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    CefMainArgs main_args(hInstance);

    // Create app to register custom schemes
    CefRefPtr<HelperApp> app = new HelperApp();

    // Execute the sub-process
    // CEF determines process type from command-line args
    return CefExecuteProcess(main_args, app, nullptr);
}

#endif  // PLATFORM_WIN
