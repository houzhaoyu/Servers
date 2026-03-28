#pragma once
#include "BaseSession.h"
#include "const.h"
#include "MsgNode.h"

class FileSession : public ProtocolSession<FileProtocol>
{
public:
    FileSession(boost::asio::io_context& io_context,
        TaskDelivery task_delivery,
        CheckSessionValidHandler check_session_valid_handler,
        RemoveSessionHandler remove_session)
        : ProtocolSession(io_context, task_delivery, check_session_valid_handler, remove_session),
        _user_uid(0)
    {
    }

    ~FileSession() {}

    void SetUserId(int uid) { _user_uid = uid; }
    int GetUserId() const { return _user_uid; }

protected:
    // ===== ProtocolSession接口实现 =====
    bool ParseHeader(const char* data, int& msg_id, int& msg_len) override;

    void OnMessage(std::shared_ptr<RecvNode> msg) override;

    void OnError() override;

private:
    int _user_uid;
};
