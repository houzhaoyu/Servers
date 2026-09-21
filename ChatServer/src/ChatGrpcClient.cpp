#include "ChatGrpcClient.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"

#include "ChatSession.h"
#include "MysqlMgr.h"
#include "Logger.h"

ChatGrpcClient::ChatGrpcClient()
{
	auto self_name = ConfigMgr::Inst().GetSelfServer().GetValue("Name");

	// 从 Redis 注册中心动态发现所有活跃的 ChatServer（替代静态 [PeerServer] 配置）
	auto active_names = RedisMgr::GetInstance()->GetActiveServerNames();
	for (auto &name : active_names)
	{
		// 跳过自身，peer 连接池仅用于跨服务器转发
		if (name == self_name)
		{
			continue;
		}

		std::string host, port, rpcport;
		if (!RedisMgr::GetInstance()->GetServerInfo(name, host, port, rpcport))
		{
			continue;
		}

		_pools[name] = std::make_unique<ChatConPool>(5, host, rpcport);
	}

	Logger::Info("ChatGrpcClient init success, peer pool count = {}", _pools.size());
}

ChatConPool *ChatGrpcClient::GetOrCreatePool(const std::string &name)
{
	{
		std::lock_guard<std::mutex> lock(_pools_mtx);
		auto it = _pools.find(name);
		if (it != _pools.end())
		{
			return it->second.get();
		}
	}

	// 池不存在，从 Redis 现查目标节点地址（锁外执行，避免阻塞其他转发）
	std::string host, port, rpcport;
	if (!RedisMgr::GetInstance()->GetServerInfo(name, host, port, rpcport))
	{
		return nullptr;
	}
	auto pool = std::make_unique<ChatConPool>(5, host, rpcport);

	std::lock_guard<std::mutex> lock(_pools_mtx);
	auto it = _pools.find(name);
	if (it != _pools.end())
	{
		// 双检：并发下可能已被其他线程建好
		return it->second.get();
	}
	auto raw = pool.get();
	_pools[name] = std::move(pool);
	return raw;
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_name, const AddFriendReq &req)
{
	Logger::Debug("NotifyAddFriend fromuid {} touid {}", req.applyuid(), req.touid());
	AddFriendRsp rsp;
	Defer defer([&rsp, &req]()
				{
		rsp.set_error(ErrorCodes::Success);
		rsp.set_applyuid(req.applyuid());
		rsp.set_touid(req.touid()); });

	auto *pool = GetOrCreatePool(server_name);
	if (pool == nullptr)
	{
		return rsp;
	}

	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, pool]()
				   { pool->returnConnection(std::move(stub)); });

	Status status = stub->NotifyAddFriend(&context, req, &rsp);

	if (!status.ok())
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, UserIdType uid, std::shared_ptr<UserInfo> &userinfo)
{
	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		Logger::Debug("User login uid is  {} name  is  pwd is  email is ", userinfo->uid, userinfo->name, userinfo->pwd, userinfo->email);
		return true;
	}
	else
	{
		// redis中没有则查询mysql
		// 查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr)
		{
			return false;
		}

		userinfo = user_info;

		// 将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
		return true;
	}
	return false;
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_name, const AuthFriendReq &req)
{
	AuthFriendRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &req]()
				{
		rsp.set_fromuid(req.fromuid());
		rsp.set_touid(req.touid()); });

	auto *pool = GetOrCreatePool(server_name);
	if (pool == nullptr)
	{
		return rsp;
	}

	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, pool]()
				   { pool->returnConnection(std::move(stub)); });

	Status status = stub->NotifyAuthFriend(&context, req, &rsp);

	if (!status.ok())
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_name,
												 const TextChatMsgReq &req, const Json::Value &rtvalue)
{

	TextChatMsgRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &req]()
				{
					rsp.set_fromuid(req.fromuid());
					rsp.set_touid(req.touid());
					for (const auto &text_data : req.textmsgs())
					{
						TextChatData *new_msg = rsp.add_textmsgs();
						new_msg->set_unique_id(text_data.unique_id());
						new_msg->set_msgcontent(text_data.msgcontent());
					} });

	auto *pool = GetOrCreatePool(server_name);
	if (pool == nullptr)
	{
		return rsp;
	}

	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, pool]()
				   { pool->returnConnection(std::move(stub)); });

	Status status = stub->NotifyTextChatMsg(&context, req, &rsp);

	if (!status.ok())
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

KickUserRsp ChatGrpcClient::NotifyKickUser(std::string server_name, const KickUserReq &req)
{
	KickUserRsp rsp;
	Defer defer([&rsp, &req]()
				{
		rsp.set_error(ErrorCodes::Success);
		rsp.set_uid(req.uid()); });

	auto *pool = GetOrCreatePool(server_name);
	if (pool == nullptr)
	{
		return rsp;
	}

	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, pool]()
				   { pool->returnConnection(std::move(stub)); });
	Status status = stub->NotifyKickUser(&context, req, &rsp);

	if (!status.ok())
	{
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}
