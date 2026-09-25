#pragma once

#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.grpc.pb.h"
#include "message.pb.h"

#include <functional>
#include <grpcpp/grpcpp.h>
#include <memory>

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

// StatusServer 异步 gRPC 客户端。Stub/Channel 支持并发调用，
// 不再为每个 RPC 占用并阻塞一个业务线程。
class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
	friend class Singleton<StatusGrpcClient>;

public:
	using GetChatServerCallback = std::function<void(GetChatServerRsp)>;
	void AsyncGetChatServer(int uid, GetChatServerCallback callback);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusService::Stub> stub_;
};
