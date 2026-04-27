# Modern CMake Patterns

## Project Structure

```cmake
cmake_minimum_required(VERSION 3.15...4.0)

project(MyProject
    VERSION 1.0.0
    DESCRIPTION "Project description"
    LANGUAGES CXX)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed")
endif()
```

## C++ Standard

```cmake
# Modern way: per-target
target_compile_features(MyTarget PRIVATE cxx_std_17)

# Alternative: project-wide
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

## Targets

### Executables

```cmake
add_executable(MyApp
    src/main.cpp
    src/app.cpp
    src/util.cpp
)

target_include_directories(MyApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(MyApp PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)
```

### Libraries

```cmake
# Static library
add_library(MyLib STATIC
    src/lib.cpp
)

# Shared library
add_library(MyShared SHARED
    src/shared.cpp
)

# Header-only (interface library)
add_library(MyHeaders INTERFACE)
target_include_directories(MyHeaders INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
```

## Linking

```cmake
# Link libraries
target_link_libraries(MyApp PRIVATE
    MyLib
    ${EXTERNAL_LIBS}
)

# Visibility keywords:
# PRIVATE   - Only this target uses it
# PUBLIC    - This target and dependents use it
# INTERFACE - Only dependents use it
```

## Finding Packages

```cmake
# Find system packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(MyApp PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# FetchContent for dependencies (CMake 3.14+)
include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

target_link_libraries(MyApp PRIVATE nlohmann_json::nlohmann_json)
```

## Compiler Warnings

```cmake
# Function to add warnings
function(add_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
    )
endfunction()

add_warnings(MyApp)
```

## Build Types

```cmake
# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Per-config settings
target_compile_definitions(MyApp PRIVATE
    $<$<CONFIG:Debug>:DEBUG_MODE>
    $<$<CONFIG:Release>:NDEBUG>
)
```

## Platform-Specific Code

```cmake
if(WIN32)
    target_sources(MyApp PRIVATE src/platform_win.cpp)
    target_link_libraries(MyApp PRIVATE user32 gdi32)
elseif(APPLE)
    target_sources(MyApp PRIVATE src/platform_mac.mm)
    find_library(COCOA_LIB Cocoa)
    target_link_libraries(MyApp PRIVATE ${COCOA_LIB})
elseif(UNIX)
    target_sources(MyApp PRIVATE src/platform_linux.cpp)
    find_package(X11 REQUIRED)
    target_link_libraries(MyApp PRIVATE ${X11_LIBRARIES})
endif()
```

## Testing

```cmake
enable_testing()

add_executable(MyTests
    tests/test_main.cpp
    tests/test_utils.cpp
)

target_link_libraries(MyTests PRIVATE MyLib)

add_test(NAME UnitTests COMMAND MyTests)
```

## Installation

```cmake
include(GNUInstallDirs)

install(TARGETS MyApp MyLib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
```

## Common Patterns

### Source File Globbing (Use with Caution)

```cmake
# Explicit listing preferred, but globbing possible:
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)
# CONFIGURE_DEPENDS re-runs glob on each build
```

### Precompiled Headers (CMake 3.16+)

```cmake
target_precompile_headers(MyApp PRIVATE
    <vector>
    <string>
    <memory>
    "include/pch.h"
)
```

### Unity Builds (CMake 3.16+)

```cmake
set_target_properties(MyApp PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)
```
