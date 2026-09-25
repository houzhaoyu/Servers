#pragma once
#include <functional>
#include <map>
#include <boost/asio/thread_pool.hpp>

#include "const.h"
#include "Singleton.h"
#include "MysqlMgr.h"


class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
	void RegGet(std::string, HttpHandler handler);
	void RegPost(std::string, HttpHandler handler);
	bool HandlePost(std::string, std::shared_ptr<HttpConnection>);
private:
	LogicSystem();
	void PostTask(std::function<void()> task);
	void PostJsonTask(std::shared_ptr<HttpConnection> connection,
		std::function<std::string()> task);
	std::map<std::string, HttpHandler> _post_handlers;
	std::map<std::string, HttpHandler> _get_handlers;
	boost::asio::thread_pool _business_pool;
};


