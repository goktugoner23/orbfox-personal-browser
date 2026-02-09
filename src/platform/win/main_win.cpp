// Windows entry point for OrbFox browser
// Handles CEF initialization, message loop, and application lifecycle

#ifdef PLATFORM_WIN

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")

#include "browser_app.h"
#include "include/cef_app.h"

// Windows entry point
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Enable High DPI support
    SetProcessDPIAware();

    // Initialize GDI+ (required for sidebar favicon rendering)
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Provide CEF with command-line arguments
    CefMainArgs main_args(hInstance);

    // CEF applications have multiple sub-processes (render, GPU, etc) that
    // share the same executable. This function checks the command-line and,
    // if this is a sub-process, executes the appropriate logic.
    CefRefPtr<BrowserApp> app(new BrowserApp);
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        // The sub-process has completed so return here.
        return exit_code;
    }

    // CEF settings
    CefSettings settings;
    settings.no_sandbox = true;  // Required for development without code signing

#ifdef DEBUG
    // Only enable remote debugging in debug builds
    settings.remote_debugging_port = 9222;
#endif

    // Set cache paths for persistent storage (cookies, localStorage, etc.)
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring orbfoxPath = std::wstring(appDataPath) + L"\\OrbFox";
        std::wstring cachePath = orbfoxPath + L"\\cache";

        // Create directories if they don't exist
        CreateDirectoryW(orbfoxPath.c_str(), NULL);
        CreateDirectoryW(cachePath.c_str(), NULL);

        // Convert to UTF-8 for CEF
        char utf8Path[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, orbfoxPath.c_str(), -1, utf8Path, sizeof(utf8Path), NULL, NULL);
        CefString(&settings.root_cache_path) = utf8Path;

        WideCharToMultiByte(CP_UTF8, 0, cachePath.c_str(), -1, utf8Path, sizeof(utf8Path), NULL, NULL);
        CefString(&settings.cache_path) = utf8Path;
    }

    // Initialize CEF
    if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
        MessageBoxW(NULL, L"Failed to initialize CEF", L"OrbFox Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Run the CEF message loop. This will block until CefQuitMessageLoop() is called.
    CefRunMessageLoop();

    // Shutdown CEF
    CefShutdown();

    // Shutdown GDI+
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}

#endif  // PLATFORM_WIN
