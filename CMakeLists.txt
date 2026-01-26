cmake_minimum_required(VERSION 3.15...4.0)

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

# Add CEF wrapper library
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Source files
set(SOURCES
    src/main.cpp
    src/client/browser_client.cpp
    src/data/window_settings.cpp
    src/data/history_storage.cpp
    src/browser/tab_manager.cpp
)

set(HEADERS
    include/browser_app.h
    include/browser_client.h
    include/window_settings.h
    include/history_storage.h
    include/tab.h
    include/workspace.h
    include/tab_manager.h
)

# Platform-specific sources
if(APPLE)
    list(APPEND SOURCES
        src/platform/mac/main_mac.mm
        src/app/browser_app.mm
        src/ui/MainWindowController.mm
        src/ui/SidebarView.mm
        src/ui/ToolbarView.mm
        src/ui/components/DesignSystem.mm
        src/ui/components/DSButton.mm
        src/ui/components/DSTextField.mm
        src/ui/components/DSRow.mm
    )
endif()

# Create executable
if(APPLE)
    add_executable(OrbFox MACOSX_BUNDLE ${SOURCES} ${HEADERS})

    set_target_properties(OrbFox PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "OrbFox"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.orbfox.browser"
    )
else()
    add_executable(OrbFox WIN32 ${SOURCES} ${HEADERS})
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
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

# Platform-specific configuration
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)

    target_link_libraries(OrbFox PRIVATE
        ${COCOA_LIBRARY}
        ${APPKIT_LIBRARY}
        sqlite3
    )

    target_compile_definitions(OrbFox PRIVATE PLATFORM_MAC)

    # Enable Objective-C++ with ARC for .mm files
    set_source_files_properties(
        src/platform/mac/main_mac.mm
        src/app/browser_app.mm
        src/ui/MainWindowController.mm
        src/ui/SidebarView.mm
        src/ui/ToolbarView.mm
        src/ui/components/DesignSystem.mm
        src/ui/components/DSButton.mm
        src/ui/components/DSTextField.mm
        src/ui/components/DSRow.mm
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
    target_link_libraries(OrbFox PRIVATE
        user32
        gdi32
        shell32
    )
    target_compile_definitions(OrbFox PRIVATE PLATFORM_WIN)
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
    add_custom_command(TARGET OrbFox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources"
            "$<TARGET_FILE_DIR:OrbFox>/Resources"
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
