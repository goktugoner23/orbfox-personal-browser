# Personal Browser - Setup Instructions

## Prerequisites

- macOS 10.15+ (Catalina or later)
- CMake 3.15+
- Xcode Command Line Tools

## Step 1: Download CEF

1. Go to: https://cef-builds.spotifycdn.com/index.html
2. Select:
   - **Platform**: macOS (or Mac ARM64 for Apple Silicon)
   - **Branch**: Stable (latest)
   - **Distribution**: Standard
3. Download the `.tar.bz2` file
4. Extract to `cef/` directory in the project root:

```bash
cd /Users/goktug/Desktop/personal-browser
mkdir -p cef
tar -xjf ~/Downloads/cef_binary_*.tar.bz2 -C cef --strip-components=1
```

The `cef/` directory should contain:
```
cef/
├── cmake/
├── include/
├── libcef_dll/
├── Release/
├── Debug/
├── Resources/
└── ...
```

## Step 2: Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Or build with parallel jobs
cmake --build build -j8
```

## Step 3: Run

```bash
# Run the app bundle
open build/PersonalBrowser.app

# Or run directly
./build/PersonalBrowser.app/Contents/MacOS/PersonalBrowser
```

## Troubleshooting

### "CEF not found" error
Make sure the `cef/` directory contains the extracted CEF distribution with the `cmake/FindCEF.cmake` file.

### Build errors about missing headers
Ensure you downloaded the Standard distribution (not Minimal) which includes headers.

### App crashes on launch
- Check that CEF framework was copied to the app bundle
- Try ad-hoc signing: `codesign --force --deep --sign - build/PersonalBrowser.app`

### Sandbox errors
The app runs with `no_sandbox = true` for development. For production, proper code signing and entitlements are required.
