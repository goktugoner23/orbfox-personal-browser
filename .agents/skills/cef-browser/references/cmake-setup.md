# CMake Setup for CEF Projects

## Complete CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.15...4.0)

# Set before project() so the compiler and SDK are configured correctly.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(MyBrowser
    VERSION 1.0
    LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# CEF Configuration
set(CEF_ROOT "${CMAKE_SOURCE_DIR}/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Add CEF wrapper library
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Source files
set(COMMON_SOURCES
    src/app/browser_app.cpp
    src/client/browser_client.cpp
    src/window/browser_window.cpp
)

# Create platform-specific executable.
if(APPLE)
    add_executable(MyBrowser MACOSX_BUNDLE
        ${COMMON_SOURCES}
        src/platform/mac/main_mac.mm
        src/window/browser_window_mac.mm
    )

    set_target_properties(MyBrowser PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "MyBrowser"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.mybrowser"
    )

elseif(WIN32)
    add_executable(MyBrowser WIN32
        ${COMMON_SOURCES}
        src/platform/win/main_win.cpp
        src/window/browser_window_win.cpp
    )

else()
    add_executable(MyBrowser
        ${COMMON_SOURCES}
        src/main.cpp
    )
endif()

target_include_directories(MyBrowser PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CEF_INCLUDE_DIRS}
)

target_link_libraries(MyBrowser PRIVATE
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

# Platform-specific configuration
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(MyBrowser PRIVATE ${COCOA_LIBRARY})
    target_compile_definitions(MyBrowser PRIVATE PLATFORM_MAC)

    # CEF on macOS requires helper app bundles inside Contents/Frameworks.
    set(HELPER_OUTPUT_NAME "MyBrowser Helper")
    foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
        string(REPLACE ":" ";" _suffix_parts ${_suffix_list})
        list(GET _suffix_parts 0 _name_suffix)
        list(GET _suffix_parts 1 _target_suffix)
        list(GET _suffix_parts 2 _plist_suffix)

        set(_helper_target "MyBrowser_Helper${_target_suffix}")
        set(_helper_output_name "${HELPER_OUTPUT_NAME}${_name_suffix}")

        add_executable(${_helper_target} MACOSX_BUNDLE
            src/platform/mac/process_helper_mac.cc
        )
        target_include_directories(${_helper_target} PRIVATE ${CEF_INCLUDE_DIRS})
        target_link_libraries(${_helper_target} PRIVATE
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
        )
        target_compile_features(${_helper_target} PRIVATE cxx_std_17)

        set(BUNDLE_ID_SUFFIX "${_plist_suffix}")
        set(EXECUTABLE_NAME "${_helper_output_name}")
        set(PRODUCT_NAME "${_helper_output_name}")
        set_target_properties(${_helper_target} PROPERTIES
            OUTPUT_NAME "${_helper_output_name}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/helper-Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "${_helper_output_name}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.mybrowser.helper${_plist_suffix}"
        )

        add_dependencies(MyBrowser ${_helper_target})
        add_custom_command(TARGET MyBrowser POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:MyBrowser>/Contents/Frameworks"
            COMMAND ditto
                "$<TARGET_BUNDLE_DIR:${_helper_target}>"
                "$<TARGET_BUNDLE_DIR:MyBrowser>/Contents/Frameworks/${_helper_output_name}.app"
            COMMENT "Copying ${_helper_output_name}.app..."
            VERBATIM
        )
    endforeach()

    add_custom_command(TARGET MyBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release/Chromium Embedded Framework.framework"
            "$<TARGET_BUNDLE_DIR:MyBrowser>/Contents/Frameworks/Chromium Embedded Framework.framework"
        COMMENT "Copying CEF framework..."
        VERBATIM
    )

elseif(WIN32)
    target_link_libraries(MyBrowser PRIVATE
        user32
        gdi32
        shell32
    )

    target_compile_definitions(MyBrowser PRIVATE PLATFORM_WIN)

    add_custom_command(TARGET MyBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources"
            "$<TARGET_FILE_DIR:MyBrowser>/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release"
            "$<TARGET_FILE_DIR:MyBrowser>"
        COMMENT "Copying CEF resources and binaries..."
        VERBATIM
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(MyBrowser PRIVATE /W4)
else()
    target_compile_options(MyBrowser PRIVATE -Wall -Wextra)
endif()
```

## Getting CEF

Download from: https://cef-builds.spotifycdn.com/index.html

Select:
- **Platform**: macOS or Windows
- **Branch**: Stable (recommended)
- **Distribution**: Standard

Extract to `cef/` directory in project root.

## Directory Structure

```
project/
├── CMakeLists.txt
├── cef/                      # CEF binary distribution
│   ├── cmake/
│   ├── include/
│   ├── libcef_dll/
│   ├── Release/
│   ├── Debug/
│   └── Resources/
├── include/
│   └── *.h
├── resources/
│   ├── Info.plist
│   └── helper-Info.plist.in
├── src/
│   ├── app/
│   ├── client/
│   ├── platform/
│   │   ├── mac/
│   │   │   ├── main_mac.mm
│   │   │   └── process_helper_mac.cc
│   │   └── win/
│   │       └── main_win.cpp
│   └── window/
└── build/
```

## Build Commands

```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Configure (Debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Build with parallel jobs
cmake --build build -j$(nproc)

# Clean and rebuild
cmake --build build --clean-first

# Build specific target
cmake --build build --target MyBrowser
```

## Troubleshooting

### CEF not found
```cmake
# Verify CEF_ROOT is correct
message(STATUS "CEF_ROOT: ${CEF_ROOT}")

# Check if cmake directory exists
if(NOT EXISTS "${CEF_ROOT}/cmake")
    message(FATAL_ERROR "CEF cmake directory not found at ${CEF_ROOT}/cmake")
endif()
```

### Missing resources at runtime
Ensure post-build commands copy:
- `Resources/` directory (ICU data, locales)
- `Release/` or `Debug/` binaries (libcef.dylib, etc.)

### macOS code signing issues
```bash
# Ad-hoc sign for development
codesign --force --deep --sign - ./build/MyBrowser.app
```
