#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TARGET_PLATFORM=${1:-hi3516cv610}

case "$TARGET_PLATFORM" in
    hi3516cv610)
        TOOLCHAIN_FILE="$SCRIPT_DIR/../cmake/toolchain-arm-v01c02.cmake"
        ;;
    rv1126b)
        TOOLCHAIN_FILE="$SCRIPT_DIR/../cmake/toolchain-rv1126b.cmake"
        ;;
    *)
        echo "Unsupported TARGET_PLATFORM: $TARGET_PLATFORM" >&2
        echo "Usage: $0 [hi3516cv610|rv1126b]" >&2
        exit 1
        ;;
esac

cd "$SCRIPT_DIR"

echo "清理旧的 build 目录..."
./clean.sh

echo "运行 CMake 配置，平台：$TARGET_PLATFORM"
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DTARGET_PLATFORM="$TARGET_PLATFORM" \
    -DCMAKE_BUILD_TYPE=Release
