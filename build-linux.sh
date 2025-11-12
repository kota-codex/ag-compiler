#!/usr/bin/env bash
set -x
set -euo pipefail

: "${VCPKG_ROOT:?Error: VCPKG_ROOT must be set to your vcpkg installation path}"
: "${ANDROID_NDK_HOME:=}"  # Required for Android builds

# update subpackages
if [ -f "update.sh" ]; then
  bash update.sh
fi

# triple         arch
TRIPLES=(
  "x64-linux     x86_64"
  "arm64-linux   arm64"
)
CONFIGS=(Release Debug)

GENERATOR="Ninja"

for tripleEntry in "${TRIPLES[@]}"; do
  read -r triple arch <<< "$tripleEntry"

  for config in "${CONFIGS[@]}"; do
    BUILD_DIR="build/${triple}"
    OUT_DIR="../../out"  # relative to build dir
    echo "Building ${triple}, config: ${config} in ${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"

    CMAKE_ARGS=(
      -S "."
      -B "${BUILD_DIR}"
      -G "${GENERATOR}"
      -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      -DVCPKG_TARGET_TRIPLET="${triple}"
      -DCMAKE_BUILD_TYPE="${config}"
      -DAG_OUT_DIR="${OUT_DIR}"
      -DAG_TRIPLE="${triple}"
      -DVCPKG_INSTALL_OPTIONS="--allow-unsupported"
    )

    if [[ "$triple" == *"android"* ]]; then
      if [[ -z "${ANDROID_NDK_HOME}" ]]; then
        echo "Error: ANDROID_NDK_HOME must be set for Android builds"
        exit 1
      fi
      CMAKE_ARGS+=(
        "-DCMAKE_SYSTEM_NAME=Android"
        "-DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME}"
        "-DCMAKE_ANDROID_ARCH_ABI=${arch}"
        "-DCMAKE_SYSTEM_VERSION=21"
      )
    fi

    cmake "${CMAKE_ARGS[@]}"
    cmake --build "${BUILD_DIR}" --parallel "$(nproc || echo 4)"
  done
done
