#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include <climits>
#include "Defer.h"
#include "Logger.h"

std::string generate_unique_string()
{
	// 创建UUID对象
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	// 将UUID转换为字符串
	std::string unique_string = to_string(uuid);

	return unique_string;
}

Status StatusServiceImpl::GetChatServer(ServerContext *context, const GetChatServerReq *request, GetChatServerRsp *reply)
{
	std::string prefix("status server has received :  ");
	const auto &server = getChatServer();
	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());
	return Status::OK;
}

StatusServiceImpl::StatusServiceImpl()
{
	// ChatServer 列表改为运行时从 Redis 注册中心动态发现，不再从配置文件静态读取
}

ChatServer StatusServiceImpl::getChatServer()
{
	Logger::Info("GetChatServer called");
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	// 利用defer解锁
	Defer defer2([this, identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	// 从 Redis 注册中心获取活跃服务器列表（已剔除心跳过期的节点）
	auto active_names = RedisMgr::GetInstance()->GetActiveServerNames();
	if (active_names.empty())
	{
		Logger::Error("No active chat server available");
		return ChatServer();
	}

	ChatServer minServer;
	minServer.name = "invalid";
	minServer.con_count = INT_MAX;
	for (auto &name : active_names)
	{
		ChatServer server;
		server.name = name;
		std::string rpcport;
		if (!RedisMgr::GetInstance()->GetServerInfo(name, server.host, server.port, rpcport))
		{
			continue;
		}

		server.con_count = INT_MAX;
		auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, name);
		if (!count_str.empty())
		{
			server.con_count = std::stoi(count_str);
		}

		if (server.con_count < minServer.con_count)
		{
			minServer = server;
		}
	}
	if (minServer.con_count == INT_MAX)
	{
		Logger::Error("No active chat server available");
		return ChatServer();
	}

	Logger::Debug("Selected chat server: {} with connection count: {}", minServer.name, minServer.con_count);

	return minServer;
}

Status StatusServiceImpl::Login(ServerContext *context, const LoginReq *request, LoginRsp *reply)
{
	Logger::Info("Login called for uid: {}", request->uid());

	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USER_TOKEN_PREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (success)
	{
		reply->set_error(ErrorCodes::UidInvalid);
		return Status::OK;
	}

	if (token_value != token)
	{
		reply->set_error(ErrorCodes::TokenInvalid);
		return Status::OK;
	}
	reply->set_error(ErrorCodes::Success);
	reply->set_uid(uid);
	reply->set_token(token);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	Logger::Info("Inserting token for uid: {}, token: {}", uid, token);

	std::string uid_str = std::to_string(uid);
	std::string token_key = USER_TOKEN_PREFIX + uid_str;
	RedisMgr::GetInstance()->Set(token_key, token);
}
