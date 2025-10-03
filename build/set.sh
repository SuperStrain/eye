#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TOOLCHAIN_FILE="$SCRIPT_DIR/../toolchain/toolchain-arm-v01c02.cmake"

cd "$SCRIPT_DIR"

echo "清理旧的 build 目录..."
./clean.sh

echo "运行 CMake 配置..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE=Release