#pragma once

#include "const.h"
#include "Singleton.h"
#include "message.grpc.pb.h"
#include "message.pb.h"

#include <functional>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
	friend class Singleton<VerifyGrpcClient>;

public:
	using VerifyCallback = std::function<void(GetVerifyRsp)>;
	void AsyncGetVerifyCode(std::string email, VerifyCallback callback);

private:
	VerifyGrpcClient();
	std::unique_ptr<VerifyService::Stub> stub_;
};
