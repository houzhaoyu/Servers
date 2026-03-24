#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"

RPConPool::RPConPool(size_t poolSize, std::string host, std::string port)
	: poolSize_(poolSize),
	host_(host),
	port_(port),
	b_stop_(false) 
{
	for (size_t i = 0; i < poolSize_; ++i) 
	{
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,
			grpc::InsecureChannelCredentials());

		connections_.push(VerifyService::NewStub(channel));
	}
}

RPConPool::~RPConPool() 
{
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty()) {
		connections_.pop();
	}
}

std::unique_ptr<VerifyService::Stub> RPConPool::getConnection() 
{
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this] {
		if (b_stop_) 
		{
			return true;
		}
		return !connections_.empty();
		});
	//如果停止则直接返回空指针
	if (b_stop_) 
	{
		return nullptr;
	}
	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void RPConPool::returnConnection(std::unique_ptr<VerifyService::Stub> context) 
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	connections_.push(std::move(context));
	cond_.notify_one();
}

void RPConPool::Close() 
{
	b_stop_ = true;
	cond_.notify_all();
}

VerifyGrpcClient::VerifyGrpcClient() {
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["VerifyServer"]["Host"];
	std::string port = gCfgMgr["VerifyServer"]["Port"];
	pool_.reset(new RPConPool(RPC_CONPOOL_SIZE, host, port));
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) {
	ClientContext context;
	GetVerifyRsp reply;
	GetVerifyReq request;
	request.set_email(email);

	auto stub = pool_->getConnection();
	Status status = stub->GetVerifyCode(&context, request, &reply);

	if (status.ok()) {
		pool_->returnConnection(std::move(stub));
		return reply;
	}
	else {
		pool_->returnConnection(std::move(stub));
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

