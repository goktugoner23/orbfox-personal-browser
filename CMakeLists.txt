cmake_minimum_required(VERSION 3.15...4.0)

# Set deployment target before project() for proper compiler/SDK configuration
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(OrbFox
    VERSION 0.1.0
    DESCRIPTION "OrbFox browser built with CEF"
    LANGUAGES CXX)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Use: cmake -B build")
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use static runtime on Windows to match CEF's libcef_dll_wrapper (/MT)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

# CEF Configuration
set(CEF_ROOT "${CMAKE_SOURCE_DIR}/cef" CACHE PATH "CEF binary distribution root")

if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
    message(FATAL_ERROR
        "CEF not found at ${CEF_ROOT}\n"
        "Download CEF from: https://cef-builds.spotifycdn.com/index.html\n"
        "Extract to: ${CMAKE_SOURCE_DIR}/cef/")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# SQLite3: use bundled amalgamation on Windows, system package elsewhere
if(WIN32)
    add_subdirectory(third_party/sqlite3)
else()
    find_package(SQLite3 REQUIRED)
endif()

# Add CEF wrapper library
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# ============================================================
# OrbFoxCore - Shared library for testable components
# ============================================================

set(CORE_SOURCES
    src/data/window_settings.cpp
    src/data/history_storage.cpp
    src/data/bookmark_storage.cpp
    src/data/session_storage.cpp
    src/data/settings_storage.cpp
    src/browser/tab_manager.cpp
    src/client/download_manager.cpp
    src/utils/filesystem_utils.cpp
    src/utils/json_utils.cpp
)

set(CORE_HEADERS
    include/window_settings.h
    include/history_storage.h
    include/bookmark_storage.h
    include/session_storage.h
    include/settings_storage.h
    include/tab.h
    include/workspace.h
    include/tab_manager.h
    include/download_manager.h
    include/utils/filesystem_utils.h
    include/utils/json_utils.h
)

add_library(OrbFoxCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(OrbFoxCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CEF_ROOT}
)

target_link_libraries(OrbFoxCore PUBLIC
    SQLite::SQLite3
)

set_target_properties(OrbFoxCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(NOT MSVC)
    target_compile_options(OrbFoxCore PRIVATE -Wall -Wextra)
endif()

# ============================================================
# OrbFox - Main application
# ============================================================

# App-specific source files (not in core library)
set(SOURCES
    src/client/browser_client.cpp
    src/client/orbfox_scheme_handler.cpp
)

set(HEADERS
    include/browser_app.h
    include/browser_client.h
    include/orbfox_scheme_handler.h
    include/bookmark_importer.h
)

# UI headers (for IDE indexing)
set(UI_HEADERS
    src/ui/MainWindowController.h
    src/ui/SidebarView.h
    src/ui/ToolbarView.h
    src/ui/BookmarkEditPopover.h
    src/ui/AutocompleteDropdown.h
    src/ui/FindBarView.h
    src/ui/components/Components.h
    src/ui/components/DesignSystem.h
    src/ui/components/DSButton.h
    src/ui/components/DSTextField.h
    src/ui/components/DSRow.h
    src/ui/components/GestureContainerView.h
    src/ui/components/ResizeHandleView.h
    src/ui/components/DevToolsDividerView.h
    src/ui/components/DevToolsClient.h
    src/ui/components/SidebarHelperViews.h
    src/ui/components/CircularProgressView.h
    src/ui/components/DownloadRowView.h
    src/ui/components/TabRowView.h
    src/ui/components/BookmarkDropContainerView.h
    src/ui/components/AddBookmarkPopoverController.h
)

# Platform-specific sources
if(APPLE)
    list(APPEND SOURCES
        src/platform/mac/main_mac.mm
        src/app/browser_app.mm
        src/ui/MainWindowController.mm
        src/ui/SidebarView.mm
        src/ui/ToolbarView.mm
        src/ui/BookmarkEditPopover.mm
        src/ui/AutocompleteDropdown.mm
        src/ui/FindBarView.mm
        src/ui/components/DesignSystem.mm
        src/ui/components/DSButton.mm
        src/ui/components/DSTextField.mm
        src/ui/components/DSRow.mm
        src/ui/components/GestureContainerView.mm
        src/ui/components/ResizeHandleView.mm
        src/ui/components/DevToolsDividerView.mm
        src/ui/components/SidebarHelperViews.mm
        src/ui/components/CircularProgressView.mm
        src/ui/components/DownloadRowView.mm
        src/ui/components/TabRowView.mm
        src/ui/components/BookmarkDropContainerView.mm
        src/ui/components/AddBookmarkPopoverController.mm
        src/data/bookmark_importer.mm
    )
elseif(WIN32)
    list(APPEND SOURCES
        src/platform/win/main_win.cpp
        src/app/browser_app_win.cpp
        src/ui/win/MainWindow.cpp
        src/ui/win/SidebarWindow.cpp
        src/ui/win/ToolbarWindow.cpp
        src/ui/win/BookmarksPanel.cpp
        src/ui/win/HistoryPanel.cpp
        src/ui/win/DownloadsPanel.cpp
        src/ui/win/FindBar.cpp
        src/ui/win/BookmarkEditDialog.cpp
        src/ui/win/AutocompleteDropdown.cpp
        src/ui/win/GestureHandler.cpp
        src/ui/win/DevToolsPanel.cpp
        src/ui/win/SettingsPanel.cpp
        resources/win/OrbFox.rc
    )

    # Windows UI headers (for IDE indexing)
    list(APPEND UI_HEADERS
        src/ui/win/MainWindow.h
        src/ui/win/SidebarWindow.h
        src/ui/win/ToolbarWindow.h
        src/ui/win/BookmarksPanel.h
        src/ui/win/HistoryPanel.h
        src/ui/win/DownloadsPanel.h
        src/ui/win/FindBar.h
        src/ui/win/BookmarkEditDialog.h
        src/ui/win/AutocompleteDropdown.h
        src/ui/win/GestureHandler.h
        src/ui/win/DevToolsPanel.h
        src/ui/win/SettingsPanel.h
        src/ui/win/DesignSystem.h
        resources/win/resource.h
    )
endif()

# Create executable
if(APPLE)
    add_executable(OrbFox MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${UI_HEADERS})

    set_target_properties(OrbFox PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "OrbFox"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.orbfox.browser"
    )
elseif(WIN32)
    add_executable(OrbFox WIN32 ${SOURCES} ${HEADERS} ${UI_HEADERS})

    set_target_properties(OrbFox PROPERTIES
        OUTPUT_NAME "OrbFox"
        WIN32_EXECUTABLE TRUE
    )

    # Embed application manifest (Common Controls v6, OS compatibility)
    set_property(TARGET OrbFox APPEND_STRING PROPERTY
        LINK_FLAGS " /MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/resources/win/OrbFox.exe.manifest\""
    )
else()
    add_executable(OrbFox ${SOURCES} ${HEADERS} ${UI_HEADERS})
endif()

# Include directories
target_include_directories(OrbFox PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/components
    ${CEF_ROOT}
)

# Link libraries
target_link_libraries(OrbFox PRIVATE
    OrbFoxCore
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

# On Windows/Linux, link against CEF shared library (not needed on macOS - uses framework)
if(CEF_LIB_RELEASE AND CEF_LIB_DEBUG)
    target_link_libraries(OrbFox PRIVATE
        optimized ${CEF_LIB_RELEASE}
        debug ${CEF_LIB_DEBUG}
    )
endif()

# Platform-specific configuration
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)

    target_link_libraries(OrbFox PRIVATE
        ${COCOA_LIBRARY}
        ${APPKIT_LIBRARY}
        ${QUARTZCORE_LIBRARY}
    )

    target_compile_definitions(OrbFox PRIVATE PLATFORM_MAC)

    # Enable Objective-C++ with ARC for .mm files
    set_source_files_properties(
        src/platform/mac/main_mac.mm
        src/app/browser_app.mm
        src/ui/MainWindowController.mm
        src/ui/SidebarView.mm
        src/ui/ToolbarView.mm
        src/ui/BookmarkEditPopover.mm
        src/ui/AutocompleteDropdown.mm
        src/ui/FindBarView.mm
        src/ui/components/DesignSystem.mm
        src/ui/components/DSButton.mm
        src/ui/components/DSTextField.mm
        src/ui/components/DSRow.mm
        src/ui/components/GestureContainerView.mm
        src/ui/components/ResizeHandleView.mm
        src/ui/components/DevToolsDividerView.mm
        src/ui/components/SidebarHelperViews.mm
        src/ui/components/CircularProgressView.mm
        src/ui/components/DownloadRowView.mm
        src/ui/components/TabRowView.mm
        src/ui/components/BookmarkDropContainerView.mm
        src/ui/components/AddBookmarkPopoverController.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc"
    )

    # ============================================================
    # Helper Apps for multi-process architecture
    # ============================================================

    set(HELPER_TARGET "OrbFox_Helper")
    set(HELPER_OUTPUT_NAME "OrbFox Helper")

    # Helper app source
    set(HELPER_SOURCES
        src/platform/mac/process_helper_mac.cc
    )

    # CEF_HELPER_APP_SUFFIXES from cef_variables.cmake:
    # Format is "<name suffix>:<target suffix>:<plist suffix>"
    # "::" - Main helper
    # " (Alerts):_alerts:.alerts"
    # " (GPU):_gpu:.gpu"
    # " (Plugin):_plugin:.plugin"
    # " (Renderer):_renderer:.renderer"

    # Create each helper app variant
    foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
        # Parse the suffix list (colon-separated)
        string(REPLACE ":" ";" _suffix_list ${_suffix_list})
        list(GET _suffix_list 0 _name_suffix)
        list(GET _suffix_list 1 _target_suffix)
        list(GET _suffix_list 2 _plist_suffix)

        # Define helper target and output names
        set(_helper_target "${HELPER_TARGET}${_target_suffix}")
        set(_helper_output_name "${HELPER_OUTPUT_NAME}${_name_suffix}")

        # Create helper executable as a bundle
        add_executable(${_helper_target} MACOSX_BUNDLE ${HELPER_SOURCES})

        # Include directories for helper
        target_include_directories(${_helper_target} PRIVATE
            ${CEF_ROOT}
        )

        # Link libraries for helper
        target_link_libraries(${_helper_target} PRIVATE
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
        )

        # Set C++ standard for helper
        set_target_properties(${_helper_target} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        # Compiler warnings for helper
        target_compile_options(${_helper_target} PRIVATE -Wall -Wextra)

        # Configure the plist with the appropriate suffix
        set(BUNDLE_ID_SUFFIX "${_plist_suffix}")
        set(EXECUTABLE_NAME "${_helper_output_name}")
        set(PRODUCT_NAME "${_helper_output_name}")

        # Set bundle properties
        set_target_properties(${_helper_target} PROPERTIES
            OUTPUT_NAME "${_helper_output_name}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/helper-Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "${_helper_output_name}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.orbfox.browser.helper${_plist_suffix}"
        )

        # Add dependency - main app depends on helper
        add_dependencies(OrbFox ${_helper_target})

        # Copy helper to main app's Frameworks directory after build
        # Use ditto which handles spaces and special characters properly
        add_custom_command(TARGET OrbFox POST_BUILD
            COMMAND ditto
                "${CMAKE_BINARY_DIR}/${_helper_output_name}.app"
                "$<TARGET_BUNDLE_DIR:OrbFox>/Contents/Frameworks/${_helper_output_name}.app"
            COMMENT "Copying ${_helper_output_name}.app to Frameworks..."
            VERBATIM
        )
    endforeach()

elseif(WIN32)
    # Windows-specific include directories
    target_include_directories(OrbFox PRIVATE
        ${CMAKE_SOURCE_DIR}/src/ui/win
    )

    # Windows libraries
    target_link_libraries(OrbFox PRIVATE
        user32
        gdi32
        gdiplus
        shell32
        dwmapi
        shlwapi
        comctl32
        ole32
        comdlg32
    )

    target_compile_definitions(OrbFox PRIVATE
        PLATFORM_WIN
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    # Use dynamic runtime to match CEF's libcef.dll
    # (default /MD is correct; do not set /MT)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(OrbFox PRIVATE /W4)
else()
    target_compile_options(OrbFox PRIVATE -Wall -Wextra)
endif()

# Copy CEF resources and binaries post-build
if(APPLE)
    # Copy framework into app bundle
    add_custom_command(TARGET OrbFox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release/Chromium Embedded Framework.framework"
            "$<TARGET_BUNDLE_DIR:OrbFox>/Contents/Frameworks/Chromium Embedded Framework.framework"
        COMMENT "Copying CEF framework..."
    )

    # Copy app icon to Resources
    add_custom_command(TARGET OrbFox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns"
            "$<TARGET_BUNDLE_DIR:OrbFox>/Contents/Resources/AppIcon.icns"
        COMMENT "Copying app icon..."
    )
else()
    # Windows: CEF expects resources (icudtl.dat, .pak files) next to the executable
    add_custom_command(TARGET OrbFox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources"
            "$<TARGET_FILE_DIR:OrbFox>"
        COMMENT "Copying CEF resources..."
    )

    add_custom_command(TARGET OrbFox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release"
            "$<TARGET_FILE_DIR:OrbFox>"
        COMMENT "Copying CEF binaries..."
    )
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CEF root: ${CEF_ROOT}")

# ============================================================
# Unit Tests
# ============================================================

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    include(FetchContent)

    # Fetch GoogleTest
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent GoogleTest from overriding parent project's compiler settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Test sources
    set(TEST_SOURCES
        tests/download_manager_test.cpp
        tests/tab_manager_test.cpp
        tests/tab_manager_integration_test.cpp
        tests/persistence_test.cpp
        tests/bookmark_storage_test.cpp
        tests/settings_storage_test.cpp
    )

    # Source files needed for testing (compiled with UNIT_TEST to disable CEF)
    # Note: Cannot use OrbFoxCore here because it's compiled with CEF headers
    set(TESTABLE_SOURCES
        src/data/window_settings.cpp
        src/data/history_storage.cpp
        src/data/bookmark_storage.cpp
        src/data/session_storage.cpp
        src/data/settings_storage.cpp
        src/browser/tab_manager.cpp
        src/client/download_manager.cpp
        src/utils/filesystem_utils.cpp
        src/utils/json_utils.cpp
    )

    # Create test executable
    add_executable(OrbFoxTests ${TEST_SOURCES} ${TESTABLE_SOURCES})

    target_include_directories(OrbFoxTests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(OrbFoxTests PRIVATE
        gtest
        gtest_main
        SQLite::SQLite3
    )

    # UNIT_TEST disables CEF dependencies in headers
    target_compile_definitions(OrbFoxTests PRIVATE UNIT_TEST)

    # Compiler warnings for tests
    if(NOT MSVC)
        target_compile_options(OrbFoxTests PRIVATE -Wall -Wextra)
    endif()

    # Enable CTest
    enable_testing()
    add_test(NAME OrbFoxTests COMMAND OrbFoxTests)

    message(STATUS "Unit tests: Enabled")
endif()
