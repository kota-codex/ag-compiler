#!/usr/bin/env bash
set -x
set -euo pipefail

: "${VCPKG_ROOT:?Error: VCPKG_ROOT must be set to your vcpkg installation path}"

# update subpackages
if [ -f "update.sh" ]; then
  bash update.sh
fi

#  arch  osxArch
TRIPLES=(
  "x64    x86_64"
  "arm64  arm64"
)

GENERATOR="Ninja"

for tripleEntry in "${TRIPLES[@]}"; do
  read -r arch osxArch <<< "$tripleEntry"
  BUILD_DIR="build/${arch}-osx"
  OUT_DIR="../../out"  # relative to build dir

  echo "Building ${arch}-osx in ${BUILD_DIR}"
  mkdir -p "${BUILD_DIR}"

  cmake -S "." -B "${BUILD_DIR}" -G "${GENERATOR}" \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_SYSTEM_NAME=Darwin \
    -DCMAKE_OSX_ARCHITECTURES="${osxArch}" \
    -DVCPKG_TARGET_TRIPLET="${arch}-osx-release" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAG_OUT_DIR="${OUT_DIR}" \
    -DAG_TRIPLE="${arch}-osx" \
    -DCMAKE_OSX_SYSROOT=macosx \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

  cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.logicalcpu)"
done
