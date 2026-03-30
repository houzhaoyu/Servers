// ResourceServer
#include "CServer.h"
#include <iostream>
#include "AsioIOContextPool.h"
#include "UserMgr.h"
#include "Logger.h"

CServer::CServer(boost::asio::io_context &io_context, unsigned int port, TaskDelivery task_delivery)
	: _io_context(io_context),
	  _port(port),
	  _acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
	  _task_delivery(task_delivery)
{
	Logger::Info("Server start listen on port : {}", _port);
	StartAccept();
}

CServer::~CServer()
{
	Logger::Info("Server destruct listen on port : {}", _port);
}

void CServer::HandleAccept(std::shared_ptr<FileSession> new_session, const boost::system::error_code &error)
{
	if (!error)
	{
		new_session->Start();
		std::lock_guard<std::mutex> lock(_mutex);
		_sessions.insert(std::make_pair(new_session->GetSessionId(), new_session));
	}
	else
	{
		Logger::Error("session accept failed, error is {}", error.what());
	}

	StartAccept();
}

void CServer::StartAccept()
{
	auto &io_context = AsioIOContextPool::GetInstance()->GetIOContext();
	std::shared_ptr<FileSession> new_session = std::make_shared<FileSession>(
		io_context, _task_delivery,
		[this](const std::string &session_id)
		{ return true; },
		[this](const std::string &session_id)
		{ this->RemoveSession(session_id); });
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1));
}

void CServer::RemoveSession(std::string session_id)
{

	if (_sessions.find(session_id) != _sessions.end())
	{
		// 移除用户和session的关联
		UserMgr::GetInstance()->RmvUserSession(_sessions[session_id]->GetUserId());
	}

	{
		std::lock_guard<std::mutex> lock(_mutex);
		_sessions.erase(session_id);
	}
}
