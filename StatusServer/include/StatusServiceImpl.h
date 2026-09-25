#pragma once

#include "message.grpc.pb.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <grpcpp/grpcpp.h>

// CallbackService 允许 Redis 回调完成后再 Finish RPC，gRPC 工作线程不会等待 Redis。
class StatusServiceImpl final : public message::StatusService::CallbackService
{
public:
	StatusServiceImpl() = default;

	grpc::ServerUnaryReactor *GetChatServer(
		grpc::CallbackServerContext *context,
		const message::GetChatServerReq *request,
		message::GetChatServerRsp *reply) override;

	grpc::ServerUnaryReactor *Login(
		grpc::CallbackServerContext *context,
		const message::LoginReq *request,
		message::LoginRsp *reply) override;
};
