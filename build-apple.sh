#!/usr/bin/env bash
set -x
set -euo pipefail

: "${VCPKG_ROOT:?Error: VCPKG_ROOT must be set to your vcpkg installation path}"

# update subpackages
if [ -f "update.sh" ]; then
  bash update.sh
fi

#  ag-triple  osxArc
TRIPLES=(
  "x64-osx    x86_64"
  "arm64-osx  arm64"
)

GENERATOR="Ninja"

for tripleEntry in "${TRIPLES[@]}"; do
  read -r triple arch <<< "$tripleEntry"

  BUILD_DIR="build/${triple}"
  OUT_DIR="../../out"  # relative to build dir
  echo "Building ${triple} in ${BUILD_DIR}"
  mkdir -p "${BUILD_DIR}"

  cmake -S "." -B "${BUILD_DIR}" -G "${GENERATOR}" \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_SYSTEM_NAME=Darwin \
    -DCMAKE_OSX_ARCHITECTURES="${arch}" \
    -DVCPKG_TARGET_TRIPLET="${triple}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAG_OUT_DIR="${OUT_DIR}" \
    -DAG_TRIPLE="${triple}" \
    -DCMAKE_OSX_SYSROOT=macosx \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

  cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.logicalcpu)"
done
