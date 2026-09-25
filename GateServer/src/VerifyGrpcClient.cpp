#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include "Logger.h"

#include <chrono>

namespace
{
	struct VerifyCall
	{
		grpc::ClientContext context;
		GetVerifyReq request;
		GetVerifyRsp response;
		VerifyGrpcClient::VerifyCallback callback;
	};
}

VerifyGrpcClient::VerifyGrpcClient()
{
	auto &cfg = ConfigMgr::Inst();
	const auto host = cfg["VerifyServer"]["Host"];
	const auto port = cfg["VerifyServer"]["Port"];
	auto channel = grpc::CreateChannel(host + ":" + port,
		grpc::InsecureChannelCredentials());
	stub_ = VerifyService::NewStub(channel);
	Logger::Info("VerifyGrpcClient async client initialized with host: {}, port: {}", host, port);
}

void VerifyGrpcClient::AsyncGetVerifyCode(std::string email, VerifyCallback callback)
{
	auto call = std::make_shared<VerifyCall>();
	call->request.set_email(std::move(email));
	call->callback = std::move(callback);
	call->context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));

	stub_->async()->GetVerifyCode(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{
			if (!status.ok())
			{
				Logger::Error("AsyncGetVerifyCode failed, grpc code: {}, message: {}",
					static_cast<int>(status.error_code()), status.error_message());
				call->response.set_error(ErrorCodes::RPCFailed);
			}
			if (call->callback)
			{
				call->callback(std::move(call->response));
			}
		});
}
