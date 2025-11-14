#!/usr/bin/env bash
set -x
set -euo pipefail

: "${VCPKG_ROOT:?Error: VCPKG_ROOT must be set to your vcpkg installation path}"

arch="$(uname -m)"
case "$arch" in
    x64|x86_64|amd64)  triple="x64-linux" ;;
    aarch64|arm64) triple="arm64-linux" ;;
    *)             triple="${arch}-linux" ;;
esac

if [ -f "update.sh" ]; then
  ./update.sh
fi
GENERATOR="Ninja"

BUILD_DIR="build/${triple}"
OUT_DIR="../../out"  # relative to build dir
echo "Building ${triple} in ${BUILD_DIR} to ${OUT_DIR}"
mkdir -p "${BUILD_DIR}"

cmake -S "." -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="${triple}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAG_OUT_DIR="${OUT_DIR}" \
  -DAG_TRIPLE="${triple}" \
  -DVCPKG_BUILD_TYPE=release
cmake --build "${BUILD_DIR}" --parallel "$(nproc || echo 4)"
