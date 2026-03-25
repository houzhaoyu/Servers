#pragma once
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <functional>
#include <iostream>
#include <cstring>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "MsgNode.h"
#include "const.h"

using boost::asio::ip::tcp;

class BaseSession {
public:
    virtual ~BaseSession() = default;

    //virtual std::string GetSessionId() const = 0;

    //virtual void Send(const std::string& msg, short msgid) = 0;
};

template<typename Protocol>
class ProtocolSession : public BaseSession, public std::enable_shared_from_this<ProtocolSession<Protocol>>
{
public:
    using SendNode = SendNodeT<Protocol>;

protected:
    tcp::socket _socket;
    std::string _session_id;

    std::atomic<bool> _b_close;

    char _data[1024 * 1024]; // 最大缓冲（可调）

    std::queue<std::shared_ptr<SendNode>> _send_que;
    std::mutex _send_lock;
    std::mutex _session_mtx;

    std::shared_ptr<RecvNode> _recv_msg_node;

    TaskDelivery _task_delivery;
    CheckSessionValidHandler _check_session_valid;
    RemoveSessionHandler _remove_session;

public:
    explicit ProtocolSession(boost::asio::io_context& io_context,
        TaskDelivery task_delivery,
        CheckSessionValidHandler check_session_valid_handler,
        RemoveSessionHandler remove_session)
        : _socket(io_context),
        _b_close(false),
        _task_delivery(task_delivery),
        _check_session_valid(check_session_valid_handler),
        _remove_session(_remove_session)
    {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        _session_id = boost::uuids::to_string(uuid);
    }

    virtual ~ProtocolSession() {
        std::cout << "~ProtocolSession destruct, id=" << _session_id << std::endl;
    }

    tcp::socket& GetSocket() { return _socket; }

    std::string& GetSessionId() { return _session_id; }

    std::shared_ptr<ProtocolSession> SharedSelf() {
        return shared_from_this();
    }

    void Start() {
        AsyncReadHead(Protocol::HEAD_LEN);
    }

    void Close() {
        std::lock_guard<std::mutex> lock(_session_mtx);
        if (_b_close) return;
        _socket.close();
        _b_close = true;
    }

    // =========================
    // 统一发送接口
    // =========================
    void Send(const char* msg, int len, short msgid) {
        std::lock_guard<std::mutex> lock(_send_lock);

        if (_send_que.size() > Protocol::MAX_SEND_QUE) {
            std::cout << "send queue full, session=" << _session_id << std::endl;
            return;
        }

        _send_que.push(std::make_shared<SendNode>(msg, len, msgid));

        if (_send_que.size() > 1) return;

        DoWrite();
    }

    void Send(const std::string& msg, short msgid) {
        Send(msg.c_str(), msg.size(), msgid);
    }

protected:
    // =========================
    // 子类必须实现
    // =========================

    // 解析头（必须返回 msg_id + msg_len）
    virtual bool ParseHeader(const char* data, int& msg_id, int& msg_len) = 0;

    // 收到完整消息
    virtual void OnMessage(std::shared_ptr<RecvNode> msg) = 0;

    // 错误处理（统一出口）
    virtual void OnError() = 0;

protected:

    void DeliverMessage()
    {
        if (_task_delivery) {
            _task_delivery(
                std::make_shared<LogicTask>(shared_from_this(), _recv_msg_node),
                _session_id
            );
        }
    }

    void RemoveSelf()
    {
        if (_remove_session)
        {
            _remove_session(_session_id);
        }
    }

    bool CheckSelfValid()
    {
        if (_check_session_valid)
        {
            return _check_session_valid(_session_id);
        }
        return false;
    }

    // =========================
    // 读流程（统一实现）
    // =========================
    void AsyncReadHead(int total_len) {
        auto self = shared_from_this();

        asyncReadFull(total_len,
            [this, self](const boost::system::error_code& ec, std::size_t bytes) {
                if (HandleError(ec, bytes, Protocol::HEAD_LEN)) return;

                int msg_id = 0;
                int msg_len = 0;

                if (!ParseHeader(_data, msg_id, msg_len)) {
                    std::cout << "invalid header\n";
                    OnError();
                    return;
                }

                if (msg_len > Protocol::MAX_LEN) {
                    std::cout << "msg too large\n";
                    OnError();
                    return;
                }

                _recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
                AsyncReadBody(msg_len);
            });
    }

    void AsyncReadBody(int total_len) {
        auto self = shared_from_this();

        asyncReadFull(total_len,
            [this, self, total_len](const boost::system::error_code& ec, std::size_t bytes) {
                if (HandleError(ec, bytes, total_len)) return;

                memcpy(_recv_msg_node->_data, _data, bytes);
                _recv_msg_node->_cur_len = bytes;
                _recv_msg_node->_data[bytes] = '\0';

                DeliverMessage();

                AsyncReadHead(Protocol::HEAD_LEN);
            });
    }

    // =========================
    // 写流程（统一实现）
    // =========================
    void DoWrite() {
        auto self = shared_from_this();
        auto& node = _send_que.front();

        boost::asio::async_write(
            _socket,
            boost::asio::buffer(node->_data, node->_total_len),
            [this, self](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cout << "write error: " << ec.message() << std::endl;
                    OnError();
                    return;
                }

                std::lock_guard<std::mutex> lock(_send_lock);
                _send_que.pop();

                if (!_send_que.empty()) {
                    DoWrite();
                }
            });
    }

    // =========================
    // 通用错误处理
    // =========================
    bool HandleError(const boost::system::error_code& ec,
        std::size_t bytes,
        std::size_t expect)
    {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            OnError();
            return true;
        }

        if (bytes < expect) {
            std::cout << "read length mismatch\n";
            OnError();
            return true;
        }

        return false;
    }

    // =========================
    // 读取指定长度
    // =========================
    void asyncReadFull(std::size_t total_len,
        std::function<void(const boost::system::error_code&, std::size_t)> handler)
    {
        memset(_data, 0, Protocol::MAX_LEN);
        asyncReadLen(0, total_len, handler);
    }

    void asyncReadLen(std::size_t read_len,
        std::size_t total_len,
        std::function<void(const boost::system::error_code&, std::size_t)> handler)
    {
        auto self = shared_from_this();

        _socket.async_read_some(
            boost::asio::buffer(_data + read_len, total_len - read_len),
            [this, self, read_len, total_len, handler]
            (const boost::system::error_code& ec, std::size_t bytes) {

                if (ec) {
                    handler(ec, read_len + bytes);
                    return;
                }

                if (read_len + bytes >= total_len) {
                    handler(ec, read_len + bytes);
                    return;
                }

                asyncReadLen(read_len + bytes, total_len, handler);
            });
    }
};