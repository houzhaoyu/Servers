#pragma once
#include <string>
#include "const.h"
#include <iostream>
#include <boost/asio.hpp>

#include <functional>

using boost::asio::ip::tcp;

class MsgNode
{
public:
	MsgNode(int max_len) :_total_len(max_len), _cur_len(0) {
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}

	~MsgNode() {
		delete[] _data;
	}

	void Clear() {
		::memset(_data, 0, _total_len);
		_cur_len = 0;
	}

	int _cur_len;
	int _total_len;
	char* _data;
};

class RecvNode : public MsgNode
{
public:
	RecvNode(int len, MsgIdType msg_id)
		: MsgNode(len), _msg_id(msg_id) {
	}

	MsgIdType _msg_id;
};

template<typename Protocol>
class SendNodeT : public MsgNode
{
public:
	SendNodeT(const char* msg, int len, MsgIdType msg_id)
		: MsgNode(len + Protocol::HEAD_LEN), _msg_id(msg_id)
	{
		Protocol::Encode(_data, msg_id, len);
		memcpy(_data + Protocol::HEAD_LEN, msg, len);
	}

	MsgIdType _msg_id;
};

using ChatSendNode = SendNodeT<ChatProtocol>;
using FileSendNode = SendNodeT<FileProtocol>;

class BaseSession;

struct LogicTask {
	std::shared_ptr<BaseSession> session;
	std::shared_ptr<RecvNode> recvnode;

	LogicTask(std::shared_ptr<BaseSession> s,
		std::shared_ptr<RecvNode> n)
		: session(s), recvnode(n) {
	}
};

typedef std::function<void(std::shared_ptr<BaseSession>, const MsgIdType&, const std::string&)> LogicHandler;

using TaskDelivery = std::function<void(std::shared_ptr<LogicTask>, const std::string&)>;

using CheckSessionValidHandler = std::function<bool(const std::string&)>;
using RemoveSessionHandler = std::function<void(const std::string&)>;
