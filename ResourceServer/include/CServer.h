#pragma once
//ResourceServer
#include <boost/asio.hpp>
#include "FileSession.h"
#include <memory.h>
#include <map>
#include <mutex>
#include "BaseLogic.h"

using boost::asio::ip::tcp;
class CServer
{
public:
	CServer(boost::asio::io_context& io_context, unsigned int port, TaskDelivery task_delivery);
	~CServer();
	void RemoveSession(std::string);
	TaskDelivery GetTaskDelivery() const { return _task_delivery; }

private:
	void HandleAccept(std::shared_ptr<FileSession>, const boost::system::error_code & error);
	void StartAccept();
	boost::asio::io_context &_io_context;
	unsigned int _port;
	tcp::acceptor _acceptor;
	std::map<std::string, std::shared_ptr<FileSession>> _sessions;
	std::mutex _mutex;
	TaskDelivery _task_delivery;
};


