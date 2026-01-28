// Helper process entry point for macOS
// Each sub-process (GPU, Renderer, etc.) runs this code

#include "include/cef_app.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_library_loader.h"

// Minimal CefApp for helper processes - just registers custom schemes
class HelperApp : public CefApp {
public:
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
};

// Entry point for CEF sub-processes
int main(int argc, char* argv[]) {
    // Load the CEF framework library at runtime
    // Required for macOS sandboxing support
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInHelper()) {
        return 1;
    }

    // Provide CEF with command-line arguments
    CefMainArgs main_args(argc, argv);

    // Create app to register custom schemes
    CefRefPtr<HelperApp> app = new HelperApp();

    // Execute the sub-process
    // CEF determines process type from command-line args
    return CefExecuteProcess(main_args, app, nullptr);
}
