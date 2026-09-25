#include "AsyncRedisClient.h"
#include "Logger.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <utility>

#include <hiredis/async.h>
#include <hiredis/adapters/poll.h>

#ifdef _WIN32
// hiredis 1.4 的 poll adapter 会引用 win32_poll，但 Windows 动态库未导出该符号。
// 直接转发到 WinSock 的 WSAPoll，保持 adapter 的非阻塞事件语义。
int win32_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	return WSAPoll(fds, nfds, timeout);
}
#endif

namespace
{
	struct CommandState
	{
		AsyncRedisClient::Callback callback;
	};

	RedisAsyncResult MakeResult(redisReply *reply)
	{
		RedisAsyncResult result;
		if (!reply)
		{
			result.error = "Redis connection was closed before a reply arrived";
			return result;
		}

		result.type = reply->type;
		if (reply->type == REDIS_REPLY_ERROR)
		{
			result.error.assign(reply->str ? reply->str : "Redis returned an error",
				reply->str ? reply->len : std::strlen("Redis returned an error"));
			return result;
		}

		result.success = reply->type != REDIS_REPLY_NIL;
		if (reply->type == REDIS_REPLY_INTEGER)
		{
			result.integer = reply->integer;
		}
		else if (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS)
		{
			result.value.assign(reply->str ? reply->str : "", reply->len);
		}
		else if (reply->type == REDIS_REPLY_ARRAY || reply->type == REDIS_REPLY_SET)
		{
			result.elements.reserve(reply->elements);
			for (std::size_t index = 0; index < reply->elements; ++index)
			{
				auto *element = reply->element[index];
				if (element && (element->type == REDIS_REPLY_STRING || element->type == REDIS_REPLY_STATUS))
				{
					result.elements.emplace_back(element->str ? element->str : "", element->len);
				}
			}
		}
		return result;
	}

	void OnCommand(redisAsyncContext *, void *raw_reply, void *private_data)
	{
		std::unique_ptr<CommandState> state(static_cast<CommandState *>(private_data));
		auto result = MakeResult(static_cast<redisReply *>(raw_reply));
		if (state->callback)
		{
			state->callback(std::move(result));
		}
	}

	void OnConnect(const redisAsyncContext *context, int status)
	{
		if (status != REDIS_OK)
		{
			Logger::Error("hiredis async connect failed: {}", context && context->errstr ? context->errstr : "unknown");
		}
	}

	void OnDisconnect(const redisAsyncContext *context, int status)
	{
		auto *client = context ? static_cast<AsyncRedisClient *>(context->data) : nullptr;
		if (client)
		{
			client->NotifyDisconnected();
		}
		if (status != REDIS_OK)
		{
			Logger::Error("hiredis async disconnected: {}", context && context->errstr ? context->errstr : "unknown");
		}
	}
}

AsyncRedisClient::AsyncRedisClient(std::string host, int port, std::string password,
	std::size_t max_pending)
	: host_(std::move(host)), port_(port), password_(std::move(password)),
	  max_pending_(max_pending), event_thread_(&AsyncRedisClient::Run, this)
{
}

AsyncRedisClient::~AsyncRedisClient()
{
	Close();
}

bool AsyncRedisClient::Command(std::vector<std::string> arguments, Callback callback)
{
	if (stopped_ || arguments.empty())
	{
		if (callback)
		{
			callback({ false, 0, 0, {}, {}, "Redis async client is stopped" });
		}
		return false;
	}

	bool queue_full = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (pending_.size() >= max_pending_)
		{
			queue_full = true;
		}
		else
		{
			pending_.push_back({ std::move(arguments), std::move(callback) });
		}
	}
	if (queue_full)
	{
		if (callback)
		{
			callback({ false, 0, 0, {}, {}, "Redis async command queue is full" });
		}
		return false;
	}
	condition_.notify_one();
	return true;
}

void AsyncRedisClient::NotifyDisconnected()
{
	disconnected_ = true;
	condition_.notify_all();
}

void AsyncRedisClient::Close()
{
	if (stopped_.exchange(true))
	{
		return;
	}
	condition_.notify_all();
	if (event_thread_.joinable())
	{
		event_thread_.join();
	}
	FailPending("Redis async client stopped");
}

void AsyncRedisClient::FailPending(const std::string &error)
{
	std::deque<PendingCommand> commands;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		commands.swap(pending_);
	}
	for (auto &command : commands)
	{
		if (command.callback)
		{
			command.callback({ false, 0, 0, {}, {}, error });
		}
	}
}

void AsyncRedisClient::Run()
{
	while (!stopped_)
	{
		disconnected_ = false;
		auto *context = redisAsyncConnect(host_.c_str(), port_);
		if (!context || context->err || redisPollAttach(context) != REDIS_OK)
		{
			Logger::Error("failed to initialize hiredis async context for {}:{}", host_, port_);
			if (context) redisAsyncFree(context);
			std::unique_lock<std::mutex> lock(mutex_);
			condition_.wait_for(lock, std::chrono::seconds(1), [this] { return stopped_.load(); });
			continue;
		}

		context->data = this;
		redisAsyncSetConnectCallback(context, OnConnect);
		redisAsyncSetDisconnectCallback(context, OnDisconnect);
		bool authenticated = password_.empty();
		if (!password_.empty())
		{
			auto *auth_state = new CommandState{
				[&authenticated](RedisAsyncResult result)
				{
					authenticated = result.success &&
						(result.value == "OK" || result.value == "ok");
					if (!authenticated)
					{
						Logger::Error("hiredis async authentication failed: {}", result.error);
					}
				}
			};
			redisAsyncCommand(context, OnCommand, auth_state, "AUTH %b",
				password_.data(), password_.size());
		}

		while (!stopped_ && !disconnected_)
		{
			if (authenticated)
			{
				std::deque<PendingCommand> commands;
				{
					std::lock_guard<std::mutex> lock(mutex_);
					commands.swap(pending_);
				}
				for (auto &command : commands)
				{
					std::vector<const char *> argv;
					std::vector<std::size_t> argvlen;
					argv.reserve(command.arguments.size());
					argvlen.reserve(command.arguments.size());
					for (const auto &argument : command.arguments)
					{
						argv.push_back(argument.data());
						argvlen.push_back(argument.size());
					}
					auto *state = new CommandState{ std::move(command.callback) };
					if (redisAsyncCommandArgv(context, OnCommand, state,
						static_cast<int>(argv.size()), argv.data(), argvlen.data()) != REDIS_OK)
					{
						std::unique_ptr<CommandState> cleanup(state);
						if (cleanup->callback)
						{
							cleanup->callback({ false, 0, 0, {}, {}, "redisAsyncCommandArgv failed" });
						}
					}
				}
			}

			redisPollTick(context, 0.01);
			if (!authenticated)
			{
				continue;
			}
			std::unique_lock<std::mutex> lock(mutex_);
			if (pending_.empty())
			{
				condition_.wait_for(lock, std::chrono::milliseconds(2),
					[this] { return stopped_.load() || !pending_.empty(); });
			}
		}

		if (!disconnected_)
		{
			redisAsyncFree(context);
		}
		if (!stopped_)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
}
