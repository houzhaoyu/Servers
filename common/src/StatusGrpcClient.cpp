#include "StatusGrpcClient.h"
#include "Logger.h"

#include <chrono>

namespace
{
	struct GetChatServerCall
	{
		grpc::ClientContext context;
		GetChatServerReq request;
		GetChatServerRsp response;
		StatusGrpcClient::GetChatServerCallback callback;
	};
}

StatusGrpcClient::StatusGrpcClient()
{
	auto &cfg = ConfigMgr::Inst();
	const auto host = cfg["StatusServer"]["Host"];
	const auto port = cfg["StatusServer"]["Port"];
	auto channel = grpc::CreateChannel(host + ":" + port,
		grpc::InsecureChannelCredentials());
	stub_ = StatusService::NewStub(channel);
	Logger::Info("StatusGrpcClient async client initialized with host: {}, port: {}", host, port);
}

void StatusGrpcClient::AsyncGetChatServer(int uid, GetChatServerCallback callback)
{
	auto call = std::make_shared<GetChatServerCall>();
	call->request.set_uid(uid);
	call->callback = std::move(callback);
	call->context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));

	stub_->async()->GetChatServer(&call->context, &call->request, &call->response,
		[call](grpc::Status status) mutable
		{
			if (!status.ok())
			{
				Logger::Error("AsyncGetChatServer failed, grpc code: {}, message: {}",
					static_cast<int>(status.error_code()), status.error_message());
				call->response.set_error(ErrorCodes::RPCFailed);
			}
			if (call->callback)
			{
				call->callback(std::move(call->response));
			}
		});
}
