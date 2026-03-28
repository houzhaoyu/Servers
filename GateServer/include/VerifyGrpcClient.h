#pragma once

#include <queue>
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"

#include "const.h"
#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

constexpr int RPC_CONPOOL_SIZE = 5;

//连接池
class RPConPool {
public:
	RPConPool(size_t poolSize, std::string host, std::string port);

	~RPConPool();

	//连接的获取和归还通过“锁+条件变量”实现
	//获取连接
	std::unique_ptr<VerifyService::Stub> getConnection();
	//归还连接
	void returnConnection(std::unique_ptr<VerifyService::Stub> context);

	void Close();

private:
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::queue<std::unique_ptr<VerifyService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
};


//GRPC验证码服务
class VerifyGrpcClient: public Singleton<VerifyGrpcClient>
{
	friend class Singleton<VerifyGrpcClient>;
public:
	GetVerifyRsp GetVerifyCode(std::string email);

private:
	VerifyGrpcClient();
	std::unique_ptr<RPConPool> pool_;
	std::unique_ptr<VerifyService::Stub> stub_;
};


