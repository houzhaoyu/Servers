#include "ChatServerGrpcClient.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "Logger.h"
#include <filesystem>

NotifyChatImgRsp  ChatServerGrpcClient::NotifyChatImgMsg(int message_id, std::string chatserver)
{
	ClientContext context;
	NotifyChatImgRsp reply;
	NotifyChatImgReq request;
	request.set_message_id(message_id);

	auto* pool_ = GetOrCreatePool(chatserver);
	if (pool_ == nullptr) {
		reply.set_error(ErrorCodes::ServerIpErr);
		return reply;
	}

	auto chat_msg = MysqlMgr::GetInstance()->GetChatMsgById(message_id);
	request.set_file_name(chat_msg->content);
	request.set_from_uid(chat_msg->sender_id);
	request.set_to_uid(chat_msg->recv_id);
	request.set_thread_id(chat_msg->thread_id);
	// 资源文件路径
	auto file_dir = ConfigMgr::Inst().GetFileOutPath();
	//该消息是接收方客户端发送过来的,服务器将资源存储在发送方的文件夹中
	auto uid_str = std::to_string(chat_msg->sender_id);
	auto file_path = (file_dir / uid_str / chat_msg->content);
	boost::uintmax_t file_size = std::filesystem::file_size(file_path);
	request.set_total_size(file_size);

	auto stub = pool_->getConnection();
	Status status = stub->NotifyChatImgMsg(&context, request, &reply);
	Defer defer([&stub, pool_]() {
		pool_->returnConnection(std::move(stub));
		});
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

ChatServerGrpcClient::ChatServerGrpcClient()
{
	// 从 Redis 注册中心动态发现所有活跃的 ChatServer（替代硬编码 chatserver1/2）
	auto active_names = RedisMgr::GetInstance()->GetActiveServerNames();
	for (auto &name : active_names)
	{
		std::string host, port, rpcport;
		if (!RedisMgr::GetInstance()->GetServerInfo(name, host, port, rpcport))
		{
			continue;
		}

		_hash_pools[name] = std::make_unique<ChatServerConPool>(5, host, rpcport);
	}

	Logger::Info("ChatServerGrpcClient init success, pool count = {}", _hash_pools.size());
}

ChatServerConPool *ChatServerGrpcClient::GetOrCreatePool(const std::string &name)
{
	{
		std::lock_guard<std::mutex> lock(_hash_pools_mtx);
		auto it = _hash_pools.find(name);
		if (it != _hash_pools.end())
		{
			return it->second.get();
		}
	}

	// 池不存在，从 Redis 现查目标节点地址（锁外执行，避免阻塞其他调用）
	std::string host, port, rpcport;
	if (!RedisMgr::GetInstance()->GetServerInfo(name, host, port, rpcport))
	{
		return nullptr;
	}
	auto pool = std::make_unique<ChatServerConPool>(5, host, rpcport);

	std::lock_guard<std::mutex> lock(_hash_pools_mtx);
	auto it = _hash_pools.find(name);
	if (it != _hash_pools.end())
	{
		// 双检：并发下可能已被其他线程建好
		return it->second.get();
	}
	auto raw = pool.get();
	_hash_pools[name] = std::move(pool);
	return raw;
}
