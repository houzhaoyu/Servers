#include "ChatSession.h"
#include <iostream>
#include "Logger.h"

// =========================
// 协议解析
// =========================
bool ChatSession::ParseHeader(const char* data, MsgIdType& msg_id, int& msg_len)
{
    memcpy(&msg_id, data, HEAD_ID_LEN);
    msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);

    if (msg_id > CHAT_MAX_LENGTH) {
        Logger::Debug("ChatSession::ParseHeader - Error - invalid msg_id: {}", msg_id);
        return false;
    }

    memcpy(&msg_len, data + HEAD_ID_LEN, CHAT_HEAD_DATA_LEN);
    msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

    if (msg_len > CHAT_MAX_LENGTH) {
        Logger::Debug("ChatSession::ParseHeader - Error - invalid msg_len: {}", msg_len);
        return false;
    }

    return true;
}

// =========================
// 收到消息（仅做增强逻辑）
// =========================
void ChatSession::OnMessage(std::shared_ptr<RecvNode> msg)
{
    // 更新心跳
    UpdateHeartbeat();

    // ⭐ 注意：投递逻辑已经在 ProtocolSession 中完成
}

// =========================
// 错误处理（统一出口）
// =========================
void ChatSession::OnError()
{
    Close();
    DealExceptionSession();
}

// =========================
// 心跳相关
// =========================
bool ChatSession::IsHeartbeatExpired(std::time_t now)
{
    double diff = std::difftime(now, _last_heartbeat);
    if (diff > HEART_THRESHOLD) {
        Logger::Debug("ChatSession::IsHeartbeatExpired - heartbeat expired: {}", GetSessionId());
        return true;
    }
    return false;
}

void ChatSession::UpdateHeartbeat()
{
    _last_heartbeat = std::time(nullptr);
}

void ChatSession::NotifyOffline(UserIdType uid) {

    Json::Value  rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["uid"] = uid;


    std::string return_str = rtvalue.toStyledString();

    Send(return_str, ID_NOTIFY_OFF_LINE_REQ);
    return;
}

void ChatSession::NotifyChatImgRecv(const ::message::NotifyChatImgReq* request) {
    Json::Value  rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["message_id"] = request->message_id();
    rtvalue["sender_id"] = request->from_uid();
    rtvalue["receiver_id"] = request->to_uid();
    rtvalue["img_name"] = request->file_name();
    rtvalue["total_size"] = std::to_string(request->total_size());
    rtvalue["thread_id"] = request->thread_id();

    std::string return_str = rtvalue.toStyledString();
    //通知图片聊天信息
    Send(return_str, ID_NOTIFY_IMG_CHAT_MSG_REQ);
    return;
}

// =========================
// Redis 清理逻辑
// =========================
void ChatSession::DealExceptionSession()
{
	// 先从本机会话表移除，连接数会立即通过 HINCRBY 回落。
	// Redis 端使用 Lua 比较 session id 后原子删除，无需同步分布式锁。
	RemoveSelf();
	if (_user_uid <= 0)
	{
		return;
	}

	auto uid_str = std::to_string(_user_uid);
	auto session_id = GetSessionId();
	static const std::string clear_session_script =
		"if redis.call('GET', KEYS[1]) == ARGV[1] then "
		"redis.call('DEL', KEYS[1]); redis.call('DEL', KEYS[2]); return 1 "
		"else return 0 end";
	RedisMgr::GetInstance()->AsyncCommand(
		{ "EVAL", clear_session_script, "2",
		  USER_SESSION_PREFIX + uid_str, USER_IP_PREFIX + uid_str, session_id },
		[session_id](RedisAsyncResult result)
		{
			if (!result.success)
			{
				Logger::Error("failed to clear redis session mapping, session = {}, error = {}",
					session_id, result.error);
			}
		});
}
