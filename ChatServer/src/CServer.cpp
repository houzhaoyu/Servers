// ChatServer
#include "CServer.h"
#include <iostream>
#include "AsioIOContextPool.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"

CServer::CServer(boost::asio::io_context &io_context,
	 unsigned int port,
	  TaskDelivery task_delivery) : _io_context(io_context),
																									   _port(port),
																									   _acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
																									   _timer(_io_context, std::chrono::seconds(HEARTBEAT_INTERVAL)),
																									   _task_delivery(task_delivery)
{
	Logger::Info("Server start success, listen on port {} ", std::to_string(_port));

	StartAccept();

	// 启动心跳定时器，用于周期刷新连接数并续租服务注册信息
	StartTimer();
}

CServer::~CServer()
{
	Logger::Info("Server destruct, listen on port {} ", std::to_string(_port));
}

void CServer::HandleAccept(std::shared_ptr<ChatSession> new_session, const boost::system::error_code &error)
{
	if (!error)
	{
		new_session->Start();
		std::size_t session_count = 0;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_sessions.insert(std::make_pair(new_session->GetSessionId(), new_session));
			session_count = _sessions.size();
		}
		Logger::Debug("new session accept, session id is {}, current session count is {}",
			new_session->GetSessionId(), session_count);
		auto self_name = ConfigMgr::Inst().GetSelfServer().GetValue("Name");
		RedisMgr::GetInstance()->AsyncHIncrBy(LOGIN_COUNT, self_name, 1,
			[self_name](RedisAsyncResult result)
			{
				if (!result.success)
				{
					Logger::Error("failed to increment connection count for {}: {}", self_name, result.error);
				}
			});
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
	std::shared_ptr<ChatSession> new_session = std::make_shared<ChatSession>(
		io_context, _task_delivery,
		[this](const std::string &session_id)
		{ return this->CheckValid(session_id); },
		[this](const std::string &session_id)
		{ this->RemoveSession(session_id); });
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1));
}

// 根据session 的id删除session，并移除用户和session的关联
void CServer::RemoveSession(std::string session_id)
{
	bool removed = false;
	std::size_t session_count = 0;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		auto found = _sessions.find(session_id);
		if (found != _sessions.end())
		{
			auto uid = found->second->GetUserId();
			UserMgr::GetInstance()->RmvUserSession(uid, session_id);
			_sessions.erase(found);
			removed = true;
		}
		session_count = _sessions.size();
	}

	if (removed)
	{
		auto self_name = ConfigMgr::Inst().GetSelfServer().GetValue("Name");
		// HINCRBY 与下限修正必须在同一 Lua 命令中原子完成。
		// 若在异步回调中再 HSET 0，迟到回调可能覆盖之后新建连接的增量。
		static const std::string decrement_script =
			"local value = redis.call('HINCRBY', KEYS[1], ARGV[1], -1); "
			"if value < 0 then redis.call('HSET', KEYS[1], ARGV[1], 0); return 0 end; "
			"return value";
		RedisMgr::GetInstance()->AsyncCommand(
			{ "EVAL", decrement_script, "1", LOGIN_COUNT, self_name },
			[self_name](RedisAsyncResult result)
			{
				if (!result.success)
				{
					Logger::Error("failed to decrement connection count for {}: {}", self_name, result.error);
				}
			});
	}
	Logger::Debug("session removed, session id is {}, current session count is {}", session_id, session_count);
}

// 根据用户获取session
std::shared_ptr<ChatSession> CServer::GetSessionBySessionId(std::string session_id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _sessions.find(session_id);
	if (it != _sessions.end())
	{
		return it->second;
	}
	return nullptr;
}

bool CServer::CheckValid(std::string session_id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _sessions.find(session_id);
	if (it != _sessions.end())
	{
		return true;
	}
	return false;
}

void CServer::on_timer(const boost::system::error_code &ec)
{
	if (ec)
	{
		Logger::Error("CServer::on_timer - timer error: {}", ec.message());
		return;
	}
	std::vector<std::shared_ptr<ChatSession>> _expired_sessions;
	int session_count = 0;
	// 此处加锁遍历session
	std::map<std::string, std::shared_ptr<ChatSession>> sessions_copy;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		sessions_copy = _sessions;
	}

	time_t now = std::time(nullptr);
	for (auto iter = sessions_copy.begin(); iter != sessions_copy.end(); iter++)
	{
		auto b_expired = iter->second->IsHeartbeatExpired(now);
		if (b_expired)
		{
			// 关闭socket, 其实这里也会触发async_read的错误处理
			iter->second->Close();
			// 收集过期信息
			_expired_sessions.push_back(iter->second);
			continue;
		}
		session_count++;
	}

	// 设置session数量
	auto self_name = ConfigMgr::Inst().GetSelfServer().GetValue("Name");
	auto count_str = std::to_string(session_count);
	RedisMgr::GetInstance()->AsyncHSet(LOGIN_COUNT, self_name, count_str,
		[self_name](RedisAsyncResult result)
		{
			if (!result.success)
			{
				Logger::Error("failed to calibrate connection count for {}: {}", self_name, result.error);
			}
		});

	// 上报心跳，续租服务注册信息（供 StatusServer 做健康检查与失活剔除）
	RedisMgr::GetInstance()->AsyncCommand(
		{ "EXPIRE", std::string(SERVER_INFO_PREFIX) + self_name, std::to_string(SERVER_INFO_TTL) },
		[self_name](RedisAsyncResult result)
		{
			if (!result.success)
			{
				Logger::Error("failed to refresh registry heartbeat for {}: {}", self_name, result.error);
			}
		});

	// 处理过期session, 单独提出，防止死锁
	for (auto &session : _expired_sessions)
	{
		session->DealExceptionSession();
	}

	// 再次设置，下一个60s检测
	_timer.expires_after(std::chrono::seconds(60));
	_timer.async_wait([this](boost::system::error_code ec)
					  { on_timer(ec); });
}

void CServer::StartTimer()
{
	// 启动定时器（用 this 捕获，与 on_timer 内部递归保持一致；
	// 不能使用 shared_from_this，因为构造函数中调用时 shared_ptr 尚未接管对象）
	_timer.async_wait([this](boost::system::error_code ec)
					  { on_timer(ec); });
}

void CServer::StopTimer()
{
	_timer.cancel();
}
