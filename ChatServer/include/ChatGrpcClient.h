#pragma once

#include "const.h"
#include "Singleton.h"
#include "data.h"
#include "message.grpc.pb.h"
#include "message.pb.h"

#include <functional>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using message::AddFriendReq;
using message::AddFriendRsp;
using message::AuthFriendReq;
using message::AuthFriendRsp;
using message::ChatService;
using message::KickUserReq;
using message::KickUserRsp;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;

class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;

public:
	using AddFriendCallback = std::function<void(AddFriendRsp)>;
	using AuthFriendCallback = std::function<void(AuthFriendRsp)>;
	using TextMessageCallback = std::function<void(TextChatMsgRsp)>;
	using KickUserCallback = std::function<void(KickUserRsp)>;

	static constexpr std::chrono::seconds ASYNC_GRPC_TIMEOUT{ 3 };

	void AsyncNotifyAddFriend(std::string server_name, AddFriendReq request,
		AddFriendCallback callback = {});
	void AsyncNotifyAuthFriend(std::string server_name, AuthFriendReq request,
		AuthFriendCallback callback = {});
	void AsyncNotifyTextChatMsg(std::string server_name, TextChatMsgReq request,
		TextMessageCallback callback = {});
	void AsyncNotifyKickUser(std::string server_name, KickUserReq request,
		KickUserCallback callback = {});

	bool GetBaseInfo(std::string base_key, UserIdType uid,
		std::shared_ptr<UserInfo> &userinfo);

private:
	ChatGrpcClient();
	std::shared_ptr<ChatService::Stub> GetOrCreateStub(const std::string &name);

	std::unordered_map<std::string, std::shared_ptr<ChatService::Stub>> _stubs;
	std::mutex _stubs_mtx;
};
