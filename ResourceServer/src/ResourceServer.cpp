#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOContextPool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include <boost/filesystem.hpp>
#include "Logger.h"

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
	auto &cfg = ConfigMgr::Inst();
	auto server_name = cfg["ResourceServer"]["Name"];
	Logger::Init(server_name);
	Logger::SetLevel(cfg["ResourceServer"]["LogLevel"]);
	Logger::Info("{} is starting...", server_name);

	std::shared_ptr<AsioIOContextPool> pool = nullptr;
	try
	{
		pool = AsioIOContextPool::GetInstance();

		boost::asio::io_context io_context;
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context, pool](auto, auto)
						   {
			io_context.stop();
			pool->Stop(); });
		auto port_str = cfg["ResourceServer"]["Port"];
		auto handler = std::bind(&LogicSystem::PostTask, LogicSystem::GetInstance().get(), std::placeholders::_1, std::placeholders::_2);
		CServer s(io_context, atoi(port_str.c_str()), handler);
		io_context.run();
	}
	catch (std::exception &e)
	{
		if (pool)
		{
			pool->Stop();
		}
		Logger::Error("Exception: {}", e.what());
	}
}
