#!/usr/bin/env bash
# ============================================================
# 一键后台启动全部服务（nohup 方式：关闭终端不中断）
# 用法： bash start_all.sh
# 日志目录： /tmp/chatlogs/
# 注意： 必须先在 build 目录存在各可执行文件，并先启动 MySQL/Redis
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
VERIFY_DIR="${SCRIPT_DIR}/VerifyServer"
LOG_DIR="/tmp/chatlogs"

mkdir -p "${LOG_DIR}"

# 检查可执行文件是否存在
check_exe() {
    local name="$1" path="$2"
    if [[ ! -x "${path}" ]]; then
        echo "[错误] 未找到可执行文件: ${path}（请先编译）"
        exit 1
    fi
}

check_exe "StatusServer"    "${BUILD_DIR}/StatusServer/StatusServer"
check_exe "ResourceServer"  "${BUILD_DIR}/ResourceServer/ResourceServer"
check_exe "ChatServer"      "${BUILD_DIR}/ChatServer/ChatServer"
check_exe "GateServer"      "${BUILD_DIR}/GateServer/GateServer"

# 每个服务的 cwd 必须在其目录内（config.ini 从当前目录读取）
start_bg() {
    local name="$1" dir="$2" pidfile="$3" logfile="$4"; shift 4
    cd "${dir}"
    nohup "$@" > "${LOG_DIR}/${logfile}" 2>&1 &
    echo $! > "${pidfile}"
    echo "==> [${name}] 已启动 (PID $(cat "${pidfile}")) 日志: ${LOG_DIR}/${logfile}"
}

echo "==== 开始启动全部服务 ===="

start_bg "VerifyServer"   "${VERIFY_DIR}"          "${LOG_DIR}/verify.pid"  "verify.log"   npm run serve
start_bg "StatusServer"   "${BUILD_DIR}/StatusServer"   "${LOG_DIR}/status.pid"   "status.log"   ./StatusServer
start_bg "ResourceServer" "${BUILD_DIR}/ResourceServer" "${LOG_DIR}/resource.pid" "resource.log" ./ResourceServer
start_bg "ChatServer1"    "${BUILD_DIR}/ChatServer"     "${LOG_DIR}/chat1.pid"    "chat1.log"    ./ChatServer -S chatserver1
start_bg "ChatServer2"    "${BUILD_DIR}/ChatServer"     "${LOG_DIR}/chat2.pid"    "chat2.log"    ./ChatServer -S chatserver2
start_bg "GateServer"     "${BUILD_DIR}/GateServer"     "${LOG_DIR}/gate.pid"     "gate.log"     ./GateServer

echo ""
echo "==== 全部启动完成 ===="
echo "查看日志： tail -f ${LOG_DIR}/*.log"
echo "查看进程： ps aux | grep -E 'ChatServer|GateServer|StatusServer|ResourceServer|node server'"
