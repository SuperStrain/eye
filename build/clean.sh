#!/bin/bash
set -e

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

echo "清理目录: $SCRIPT_DIR"

# 遍历目录下的文件和文件夹
for f in "$SCRIPT_DIR"/*; do
    # 如果是 .sh 文件则跳过
    if [[ "$f" == *.sh ]]; then
        echo "保留: $f"
        continue
    fi

    # 删除其他文件或文件夹
    echo "删除: $f"
    rm -rf "$f"
done

echo "✅ 清理完成"

