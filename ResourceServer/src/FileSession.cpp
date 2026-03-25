#include "FileSession.h"
#include <iostream>

// =========================
// 协议解析（注意：int长度）
// =========================
bool FileSession::ParseHeader(const char* data, int& msg_id, int& msg_len)
{
    memcpy(&msg_id, data, HEAD_ID_LEN);
    msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);

    if (msg_id > FILE_MAX_LENGTH) {
        std::cout << "invalid msg_id: " << msg_id << std::endl;
        return false;
    }

    memcpy(&msg_len, data + HEAD_ID_LEN, FILE_HEAD_DATA_LEN);
    msg_len = boost::asio::detail::socket_ops::network_to_host_long(msg_len);

    if (msg_len > FILE_MAX_LENGTH) {
        std::cout << "invalid msg_len: " << msg_len << std::endl;
        return false;
    }

    return true;
}

// =========================
// 收到消息（无需重复投递）
// =========================
void FileSession::OnMessage(std::shared_ptr<RecvNode> msg)
{
    // ProtocolSession 已经完成任务投递
    // 如果需要额外逻辑（如日志/限流），可以写在这里
}

// =========================
// 错误处理
// =========================
void FileSession::OnError()
{
    Close();
    RemoveSelf();
    // ResourceServer 不涉及 Redis / 心跳
    // 如果你后面需要清理逻辑，可以在这里补充
}