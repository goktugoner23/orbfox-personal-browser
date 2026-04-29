#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Build OrbFox's pinned macOS CEF distribution with proprietary codec support.

This script downloads Chromium/CEF source and runs upstream automate-git.py.
It does not modify the repo-local cef/ directory.

Usage:
  scripts/build-cef-proprietary-macos.sh [--preflight-only] [--print-command]

Environment overrides:
  BUILD_ROOT          Work directory outside the repo.
                      Default: $HOME/code/orbfox-cef-proprietary
                      Use an external drive for real builds, for example:
                      /Volumes/OrbFoxCEF/orbfox-cef-proprietary
  CEF_BUILD_ARCH      arm64 or x64. Default: host architecture.
  MIN_FREE_GB         Required free disk space before starting. Default: 200.
  CEF_BRANCH          CEF source branch. Default: 7559.
  CEF_CHECKOUT        Exact CEF commit. Default: e135be2c924b8368b95c4d637528fe3604f70df9.
  GN_DEFINES          GN flags. Default enables official build plus H.264/AAC.
  FORCE_CLEAN         Set to 1 to force-clean Chromium/CEF checkouts. Default: 0.
  WITH_PGO_PROFILES   Set to 0 to skip Chromium PGO profile download. Default: 1.

Output:
  $BUILD_ROOT/chromium_git/chromium/src/cef/binary_distrib/
USAGE
}

die() {
  echo "error: $*" >&2
  exit 1
}

require_tool() {
  command -v "$1" >/dev/null 2>&1 || die "Missing required tool: $1"
}

path_without_spaces() {
  [[ "$1" != *" "* ]] || die "$2 must not contain spaces: $1"
}

nearest_existing_path() {
  local path="$1"
  while [[ ! -e "$path" ]]; do
    path="$(dirname "$path")"
  done
  printf '%s\n' "$path"
}

available_gb_for_path() {
  local path
  path="$(nearest_existing_path "$1")"
  local available_kb
  available_kb="$(df -Pk "$path" | awk 'NR == 2 { print $4 }')"
  printf '%d\n' "$((available_kb / 1024 / 1024))"
}

print_command() {
  printf 'export CEF_USE_GN=%q\n' "$CEF_USE_GN"
  printf 'export GN_DEFINES=%q\n' "$GN_DEFINES"
  printf 'export CEF_ARCHIVE_FORMAT=%q\n' "$CEF_ARCHIVE_FORMAT"
  printf 'export SDKROOT=%q\n' "$SDKROOT"
  printf 'export PATH=%q:$PATH\n' "$DEPOT_TOOLS_DIR"
  if [[ -n "${CEF_ENABLE_AMD64:-}" ]]; then
    printf 'export CEF_ENABLE_AMD64=%q\n' "$CEF_ENABLE_AMD64"
  fi
  if [[ -n "${CEF_ENABLE_ARM64:-}" ]]; then
    printf 'export CEF_ENABLE_ARM64=%q\n' "$CEF_ENABLE_ARM64"
  fi

  printf 'python3'
  printf ' %q' "${AUTOMATE_CMD[@]}"
  printf '\n'
}

PREFLIGHT_ONLY=0
PRINT_COMMAND=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preflight-only)
      PREFLIGHT_ONLY=1
      ;;
    --print-command)
      PRINT_COMMAND=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "Unknown argument: $1"
      ;;
  esac
  shift
done

[[ "$(uname -s)" == "Darwin" ]] || die "This script is macOS-only."

CEF_BRANCH="${CEF_BRANCH:-7559}"
CEF_CHECKOUT="${CEF_CHECKOUT:-e135be2c924b8368b95c4d637528fe3604f70df9}"
BUILD_ROOT="${BUILD_ROOT:-$HOME/code/orbfox-cef-proprietary}"
AUTOMATE_DIR="${AUTOMATE_DIR:-$BUILD_ROOT/automate}"
DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-$BUILD_ROOT/depot_tools}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-$BUILD_ROOT/chromium_git}"
AUTOMATE_SCRIPT="$AUTOMATE_DIR/automate-git.py"
MIN_FREE_GB="${MIN_FREE_GB:-200}"
FORCE_CLEAN="${FORCE_CLEAN:-0}"
WITH_PGO_PROFILES="${WITH_PGO_PROFILES:-1}"
CEF_ARCHIVE_FORMAT="${CEF_ARCHIVE_FORMAT:-tar.bz2}"
CEF_USE_GN="${CEF_USE_GN:-1}"

case "${CEF_BUILD_ARCH:-$(uname -m)}" in
  arm64)
    CEF_BUILD_ARCH="arm64"
    ARCH_FLAG="--arm64-build"
    ;;
  x64|x86_64|amd64)
    CEF_BUILD_ARCH="x64"
    ARCH_FLAG="--x64-build"
    ;;
  *)
    die "Unsupported CEF_BUILD_ARCH: ${CEF_BUILD_ARCH:-$(uname -m)}"
    ;;
esac

HOST_ARCH="$(uname -m)"
if [[ "$CEF_BUILD_ARCH" == "x64" && "$HOST_ARCH" == "arm64" ]]; then
  export CEF_ENABLE_AMD64="${CEF_ENABLE_AMD64:-1}"
elif [[ "$CEF_BUILD_ARCH" == "arm64" && "$HOST_ARCH" == "x86_64" ]]; then
  export CEF_ENABLE_ARM64="${CEF_ENABLE_ARM64:-1}"
fi

DEFAULT_GN_DEFINES="is_official_build=true proprietary_codecs=true ffmpeg_branding=Chrome symbol_level=0 blink_symbol_level=0 v8_symbol_level=0"
GN_DEFINES="${GN_DEFINES:-$DEFAULT_GN_DEFINES}"
SDKROOT="${SDKROOT:-$(xcrun -show-sdk-path -sdk macosx)}"

path_without_spaces "$BUILD_ROOT" "BUILD_ROOT"
path_without_spaces "$DEPOT_TOOLS_DIR" "DEPOT_TOOLS_DIR"
path_without_spaces "$DOWNLOAD_DIR" "DOWNLOAD_DIR"

require_tool git
require_tool python3
require_tool curl
require_tool xcode-select
require_tool xcrun

xcode-select -p >/dev/null
xcrun --sdk macosx --show-sdk-path >/dev/null

AVAILABLE_GB="$(available_gb_for_path "$BUILD_ROOT")"
if (( AVAILABLE_GB < MIN_FREE_GB )); then
  die "Only ${AVAILABLE_GB} GiB free for $BUILD_ROOT; CEF source builds need at least ${MIN_FREE_GB} GiB. Set BUILD_ROOT to an external volume, for example BUILD_ROOT=/Volumes/OrbFoxCEF/orbfox-cef-proprietary."
fi

AUTOMATE_CMD=(
  "$AUTOMATE_SCRIPT"
  "--download-dir=$DOWNLOAD_DIR"
  "--depot-tools-dir=$DEPOT_TOOLS_DIR"
  "--branch=$CEF_BRANCH"
  "--checkout=$CEF_CHECKOUT"
  --no-debug-build
  --force-build
  --force-distrib
  --no-distrib-docs
  --no-chromium-history
  --build-log-file
  "$ARCH_FLAG"
)

if [[ "$FORCE_CLEAN" == "1" ]]; then
  AUTOMATE_CMD+=(--force-clean)
fi

if [[ "$WITH_PGO_PROFILES" == "1" ]]; then
  AUTOMATE_CMD+=(--with-pgo-profiles)
fi

export CEF_USE_GN
export GN_DEFINES
export CEF_ARCHIVE_FORMAT
export SDKROOT
export PATH="$DEPOT_TOOLS_DIR:$PATH"

echo "CEF branch: $CEF_BRANCH"
echo "CEF checkout: $CEF_CHECKOUT"
echo "CEF architecture: $CEF_BUILD_ARCH"
echo "Build root: $BUILD_ROOT"
echo "GN_DEFINES: $GN_DEFINES"

if [[ "$PRINT_COMMAND" == "1" ]]; then
  print_command
fi

if [[ "$PREFLIGHT_ONLY" == "1" || "$PRINT_COMMAND" == "1" ]]; then
  echo "Preflight complete; build not started."
  exit 0
fi

mkdir -p "$AUTOMATE_DIR"

if [[ ! -d "$DEPOT_TOOLS_DIR/.git" ]]; then
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
fi

curl -fsSL \
  https://raw.githubusercontent.com/chromiumembedded/cef/master/tools/automate/automate-git.py \
  -o "$AUTOMATE_SCRIPT"
chmod 755 "$AUTOMATE_SCRIPT"

python3 "${AUTOMATE_CMD[@]}"

echo "CEF binary distributions:"
find "$DOWNLOAD_DIR/chromium/src/cef/binary_distrib" -maxdepth 1 -type f -name 'cef_binary_*' -print
