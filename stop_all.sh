#!/usr/bin/env bash
# ============================================================
# 一键停止全部服务
# 用法： bash stop_all.sh
# ============================================================

set -uo pipefail

LOG_DIR="/tmp/chatlogs"

echo "==== 停止全部服务 ===="

# 停止 VerifyServer (node server.js)
if pkill -f "node server.js" 2>/dev/null; then
    echo "==> VerifyServer 已停止"
else
    echo "==> VerifyServer 未在运行"
fi

# 停止 C++ 服务（按可执行文件名精确匹配）
for svc in "StatusServer/StatusServer" "ResourceServer/ResourceServer" "ChatServer/ChatServer" "GateServer/GateServer"; do
    if pkill -f "build/${svc}" 2>/dev/null; then
        echo "==> ${svc} 已停止"
    else
        echo "==> ${svc} 未在运行"
    fi
done

# 清理 pid 文件
rm -f "${LOG_DIR}"/*.pid 2>/dev/null || true

echo "==== 全部停止完成 ===="
echo "确认： ps aux | grep -E 'ChatServer|GateServer|StatusServer|ResourceServer|node server' | grep -v grep"
