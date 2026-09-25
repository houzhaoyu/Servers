#include "ChatGrpcClient.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"

#include <chrono>

namespace
{
	template <typename Request, typename Response, typename Callback>
	struct AsyncChatCall
	{
		grpc::ClientContext context;
		Request request;
		Response response;
		Callback callback;
		std::shared_ptr<ChatService::Stub> stub;
	};

	template <typename Response, typename Callback>
	void CompleteChatCall(const grpc::Status &status, Response &response,
		Callback &callback, const char *method)
	{
		if (!status.ok())
		{
			Logger::Error("{} async rpc failed, grpc code: {}, message: {}", method,
				static_cast<int>(status.error_code()), status.error_message());
			response.set_error(ErrorCodes::RPCFailed);
		}
		if (callback)
		{
			callback(std::move(response));
		}
	}
}

ChatGrpcClient::ChatGrpcClient()
{
	auto self_name = ConfigMgr::Inst().GetSelfServer().GetValue("Name");
	auto active_names = RedisMgr::GetInstance()->GetActiveServerNames();
	for (auto &name : active_names)
	{
		if (name != self_name)
		{
			GetOrCreateStub(name);
		}
	}
	Logger::Info("ChatGrpcClient async client initialized, peer count = {}", _stubs.size());
}

std::shared_ptr<ChatService::Stub> ChatGrpcClient::GetOrCreateStub(const std::string &name)
{
	{
		std::lock_guard<std::mutex> lock(_stubs_mtx);
		auto it = _stubs.find(name);
		if (it != _stubs.end())
		{
			return it->second;
		}
	}

	std::string host, port, rpcport;
	if (!RedisMgr::GetInstance()->GetServerInfo(name, host, port, rpcport))
	{
		return nullptr;
	}
	auto channel = grpc::CreateChannel(host + ":" + rpcport,
		grpc::InsecureChannelCredentials());
	auto created = std::shared_ptr<ChatService::Stub>(ChatService::NewStub(channel).release());

	std::lock_guard<std::mutex> lock(_stubs_mtx);
	auto inserted = _stubs.emplace(name, created);
	return inserted.first->second;
}

void ChatGrpcClient::AsyncNotifyAddFriend(std::string server_name, AddFriendReq request,
	AddFriendCallback callback)
{
	using Call = AsyncChatCall<AddFriendReq, AddFriendRsp, AddFriendCallback>;
	auto call = std::make_shared<Call>();
	call->stub = GetOrCreateStub(server_name);
	call->request = std::move(request);
	call->callback = std::move(callback);
	call->response.set_applyuid(call->request.applyuid());
	call->response.set_touid(call->request.touid());
	if (!call->stub)
	{
		call->response.set_error(ErrorCodes::RPCFailed);
		if (call->callback) call->callback(std::move(call->response));
		return;
	}
	call->context.set_deadline(std::chrono::system_clock::now() + ASYNC_GRPC_TIMEOUT);
	call->stub->async()->NotifyAddFriend(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{ CompleteChatCall(status, call->response, call->callback, "NotifyAddFriend"); });
}

void ChatGrpcClient::AsyncNotifyAuthFriend(std::string server_name, AuthFriendReq request,
	AuthFriendCallback callback)
{
	using Call = AsyncChatCall<AuthFriendReq, AuthFriendRsp, AuthFriendCallback>;
	auto call = std::make_shared<Call>();
	call->stub = GetOrCreateStub(server_name);
	call->request = std::move(request);
	call->callback = std::move(callback);
	call->response.set_fromuid(call->request.fromuid());
	call->response.set_touid(call->request.touid());
	if (!call->stub)
	{
		call->response.set_error(ErrorCodes::RPCFailed);
		if (call->callback) call->callback(std::move(call->response));
		return;
	}
	call->context.set_deadline(std::chrono::system_clock::now() + ASYNC_GRPC_TIMEOUT);
	call->stub->async()->NotifyAuthFriend(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{ CompleteChatCall(status, call->response, call->callback, "NotifyAuthFriend"); });
}

void ChatGrpcClient::AsyncNotifyTextChatMsg(std::string server_name, TextChatMsgReq request,
	TextMessageCallback callback)
{
	using Call = AsyncChatCall<TextChatMsgReq, TextChatMsgRsp, TextMessageCallback>;
	auto call = std::make_shared<Call>();
	call->stub = GetOrCreateStub(server_name);
	call->request = std::move(request);
	call->callback = std::move(callback);
	call->response.set_fromuid(call->request.fromuid());
	call->response.set_touid(call->request.touid());
	if (!call->stub)
	{
		call->response.set_error(ErrorCodes::RPCFailed);
		if (call->callback) call->callback(std::move(call->response));
		return;
	}
	call->context.set_deadline(std::chrono::system_clock::now() + ASYNC_GRPC_TIMEOUT);
	call->stub->async()->NotifyTextChatMsg(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{ CompleteChatCall(status, call->response, call->callback, "NotifyTextChatMsg"); });
}

void ChatGrpcClient::AsyncNotifyKickUser(std::string server_name, KickUserReq request,
	KickUserCallback callback)
{
	using Call = AsyncChatCall<KickUserReq, KickUserRsp, KickUserCallback>;
	auto call = std::make_shared<Call>();
	call->stub = GetOrCreateStub(server_name);
	call->request = std::move(request);
	call->callback = std::move(callback);
	call->response.set_uid(call->request.uid());
	if (!call->stub)
	{
		call->response.set_error(ErrorCodes::RPCFailed);
		if (call->callback) call->callback(std::move(call->response));
		return;
	}
	call->context.set_deadline(std::chrono::system_clock::now() + ASYNC_GRPC_TIMEOUT);
	call->stub->async()->NotifyKickUser(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{ CompleteChatCall(status, call->response, call->callback, "NotifyKickUser"); });
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, UserIdType uid,
	std::shared_ptr<UserInfo> &userinfo)
{
	std::string info_str;
	if (RedisMgr::GetInstance()->Get(base_key, info_str))
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
		return true;
	}

	auto user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (!user_info)
	{
		return false;
	}
	userinfo = user_info;
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
