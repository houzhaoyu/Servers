#pragma once
#include "BaseSession.h"
#include "const.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Defer.h"
#include <atomic>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include "MsgNode.h"

class ChatSession : public ProtocolSession<ChatProtocol>
{
public:
    ChatSession(boost::asio::io_context& io_context,
        TaskDelivery task_delivery,
        CheckSessionValidHandler check_session_valid_handler,
        RemoveSessionHandler remove_session)
        : ProtocolSession(io_context, task_delivery, check_session_valid_handler, remove_session),
        _user_uid(0)
    {
        _last_heartbeat = std::time(nullptr);
    }

    ~ChatSession() {}

    void SetUserId(int uid) { _user_uid = uid; }
    int GetUserId() const { return _user_uid; }

    // 心跳检测
    bool IsHeartbeatExpired(std::time_t now);

    void UpdateHeartbeat();

    void NotifyOffline(int uid);
    void NotifyChatImgRecv(const ::message::NotifyChatImgReq* request);

    void DealExceptionSession();

protected:
    // ===== ProtocolSession接口实现 =====
    bool ParseHeader(const char* data, int& msg_id, int& msg_len) override;

    void OnMessage(std::shared_ptr<RecvNode> msg) override;

    void OnError() override;

private:
    int _user_uid;
    std::atomic<time_t> _last_heartbeat;
};