#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct RedisAsyncResult
{
	bool success{ false };
	int type{ 0 };
	long long integer{ 0 };
	std::string value;
	std::vector<std::string> elements;
	std::string error;
};

// hiredis async 客户端。所有 redisAsyncCommandArgv 调用都在同一个事件线程执行，
// 调用方线程只负责入队，不会等待 Redis 网络 I/O。
class AsyncRedisClient
{
public:
	using Callback = std::function<void(RedisAsyncResult)>;

	AsyncRedisClient(std::string host, int port, std::string password,
		std::size_t max_pending = 10000);
	~AsyncRedisClient();

	AsyncRedisClient(const AsyncRedisClient &) = delete;
	AsyncRedisClient &operator=(const AsyncRedisClient &) = delete;

	bool Command(std::vector<std::string> arguments, Callback callback);
	void Close();
	void NotifyDisconnected();

private:
	struct PendingCommand
	{
		std::vector<std::string> arguments;
		Callback callback;
	};

	void Run();
	void FailPending(const std::string &error);

	std::string host_;
	int port_;
	std::string password_;
	std::size_t max_pending_;
	std::atomic<bool> stopped_{ false };
	std::atomic<bool> disconnected_{ false };
	std::mutex mutex_;
	std::condition_variable condition_;
	std::deque<PendingCommand> pending_;
	std::thread event_thread_;
};
