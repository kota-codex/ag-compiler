#!/usr/bin/env bash
set -x
set -euo pipefail

: "${VCPKG_ROOT:?Error: VCPKG_ROOT must be set to your vcpkg installation path}"

arch="$(uname -m)"
case "$arch" in
    x64|x86_64|amd64) arch="x64" ;;
    aarch64|arm64)    arch="arm64" ;;
esac

if [ -f "update.sh" ]; then
  ./update.sh
fi
GENERATOR="Ninja"

BUILD_DIR="build/${arch}-linux"
OUT_DIR="../../out"  # relative to build dir
echo "Building ${arch}-linux in ${BUILD_DIR} to ${OUT_DIR}"
mkdir -p "${BUILD_DIR}"
cp -u "rel-triple-${arch}.cmake" "${VCPKG_ROOT}triplets/community/${arch}-linux-rel.cmake"

cmake -S "." -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="${arch}-linux-rel" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAG_OUT_DIR="${OUT_DIR}" \
  -DAG_TRIPLE="${arch}-linux"

cmake --build "${BUILD_DIR}" --parallel "$(nproc || echo 4)"
