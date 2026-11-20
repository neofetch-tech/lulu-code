#!/usr/bin/env bash
# Build Lulu Code on Arch or Debian/Ubuntu.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-linux"

if command -v apt-get &>/dev/null; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake git qt6-base-dev qt6-svg-dev
elif command -v pacman &>/dev/null; then
  sudo pacman -S --needed --noconfirm base-devel cmake git qt6-base qt6-svg
else
  echo "Unsupported package manager. Install Qt 6 Core, Gui, Widgets, Network, Svg, CMake, and Git." >&2
  exit 1
fi

cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLULU_WITH_LLAMA=ON \
  -DLULU_FETCH_LLAMA=ON \
  -DLULU_LLAMA_TAG=b10333 \
  -DLULU_BUILD_TESTS=OFF
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

DIST_DIR="${PROJECT_DIR}/dist"
PACKAGE_DIR="${DIST_DIR}/lulu-code-linux"
mkdir -p "${PACKAGE_DIR}"
install -m 755 "${BUILD_DIR}/lulu_code" "${PACKAGE_DIR}/lulu_code"
cat > "${PACKAGE_DIR}/README.txt" <<'EOF'
Lulu Code for Linux

Run ./lulu_code from a desktop session. Qt 6.5+ (Core, Gui, Widgets,
Network and Svg) and the usual Linux desktop libraries must be installed.
EOF
ARCHIVE="${DIST_DIR}/lulu-code-linux.tar.gz"
tar -czf "${ARCHIVE}" -C "${DIST_DIR}" lulu-code-linux
printf 'Build: %s\nArchive: %s\n' "${PACKAGE_DIR}/lulu_code" "${ARCHIVE}"