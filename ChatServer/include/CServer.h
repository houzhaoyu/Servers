#pragma once
//ChatServer
#include <boost/asio.hpp>
#include "ChatSession.h"
#include <memory.h>
#include <map>
#include <mutex>
#include <boost/asio/steady_timer.hpp>
#include "BaseLogic.h"

using boost::asio::ip::tcp;

class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& io_context, unsigned int port, TaskDelivery task_delivery);
	~CServer();
	void RemoveSession(std::string);
	//根据session_id获取session
	std::shared_ptr<ChatSession> GetSessionBySessionId(std::string);
	TaskDelivery GetTaskDelivery() const { return _task_delivery; }
	bool CheckValid(std::string);
	void on_timer(const boost::system::error_code& ec);
	void StartTimer();
	void StopTimer();
private:
	void HandleAccept(std::shared_ptr<ChatSession>, const boost::system::error_code & error);
	void StartAccept();
	boost::asio::io_context &_io_context;
	unsigned int _port;
	tcp::acceptor _acceptor;
	//session_id与session的映射
	std::map<std::string, std::shared_ptr<ChatSession>> _sessions;
	std::mutex _mutex;
	boost::asio::steady_timer _timer;
	TaskDelivery _task_delivery;
};


