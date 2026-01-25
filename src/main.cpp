#include "browser_app.h"

#include "include/cef_app.h"
#include "include/cef_command_line.h"

#if defined(PLATFORM_MAC)
// macOS entry point is in main_mac.mm
#elif defined(PLATFORM_WIN)

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CefMainArgs main_args(hInstance);

    // Check if this is a subprocess
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    // CEF settings
    CefSettings settings;
    settings.no_sandbox = true;  // Required for development

    // Create the application
    CefRefPtr<BrowserApp> app(new BrowserApp);

    // Initialize CEF
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return 1;
    }

    // Run the message loop
    CefRunMessageLoop();

    // Shutdown CEF
    CefShutdown();

    return 0;
}

#else
// Fallback for other platforms
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefSettings settings;
    settings.no_sandbox = true;

    CefRefPtr<BrowserApp> app(new BrowserApp);

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return 1;
    }

    CefRunMessageLoop();
    CefShutdown();

    return 0;
}
#endif
