#!/usr/bin/env bash
# ============================================================
# 将根目录 config.ini 复制到各服务 build 目录
# 用法： bash copy_config.sh
# 说明： 复制前会先备份目标目录已有的 config.ini 为 config.ini.bak
# ============================================================

set -euo pipefail

# 脚本所在目录（即 Servers/）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_CONFIG="${SCRIPT_DIR}/config.ini"

# build 根目录
BUILD_DIR="${SCRIPT_DIR}/build"

# 需要同步配置的服务目录
TARGETS=(GateServer ChatServer StatusServer ResourceServer)

if [[ ! -f "${SRC_CONFIG}" ]]; then
    echo "[错误] 未找到源配置文件: ${SRC_CONFIG}"
    exit 1
fi

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "[错误] 未找到 build 目录: ${BUILD_DIR}"
    exit 1
fi

for target in "${TARGETS[@]}"; do
    dest_dir="${BUILD_DIR}/${target}"
    if [[ ! -d "${dest_dir}" ]]; then
        echo "[跳过] 目标目录不存在: ${dest_dir}"
        continue
    fi

    # 备份旧的 config.ini（如有）
    if [[ -f "${dest_dir}/config.ini" ]]; then
        cp "${dest_dir}/config.ini" "${dest_dir}/config.ini.bak"
    fi

    cp "${SRC_CONFIG}" "${dest_dir}/config.ini"
    echo "[完成] ${SRC_CONFIG}"
    echo "        -> ${dest_dir}/config.ini"
done

echo "===================="
echo "全部完成，共处理 ${#TARGETS[@]} 个目标目录"
