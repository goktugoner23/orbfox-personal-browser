// Helper process entry point for macOS
// Each sub-process (GPU, Renderer, etc.) runs this code

#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"

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

    // Execute the sub-process
    // CEF determines process type from command-line args
    return CefExecuteProcess(main_args, nullptr, nullptr);
}
