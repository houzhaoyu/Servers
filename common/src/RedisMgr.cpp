#include "RedisMgr.h"
#include "const.h"
#include "ConfigMgr.h"
#include "Defer.h"
#include "DistLock.h"
#include "Logger.h"

RedisMgr::RedisMgr()
{
	auto &gCfgMgr = ConfigMgr::Inst();
	auto host = gCfgMgr["Redis"]["Host"];
	auto port = gCfgMgr["Redis"]["Port"];
	auto pwd = gCfgMgr["Redis"]["Passwd"];
	auto con_num = gCfgMgr["Redis"]["ConPoolNum"];
	_con_pool.reset(new RedisConPool(atoi(con_num.c_str()), host.c_str(), atoi(port.c_str()), pwd.c_str()));
	Logger::Info("RedisMgr start! host = {}, port = {}, conn_num = {}", host, port, con_num);
}

RedisMgr::~RedisMgr()
{
	Close();
}

bool RedisMgr::Get(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "GET %s", key.c_str());
	if (reply == NULL)
	{
		Logger::Debug("RedisMgr::Get, Failed : [GET {} ]", key);
		// freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING)
	{
		Logger::Debug("RedisMgr::Get, Failed : [GET {} ]", key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);

	Logger::Debug("RedisMgr::Get, Succeed : [GET {} ]", key);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
	// 执行redis命令行
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	// 如果返回NULL则说明执行失败
	if (NULL == reply)
	{
		Logger::Debug("RedisMgr::Set, Failed : [SET {} {} ]", key, value);
		// freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	// 如果执行失败则释放连接
	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		Logger::Debug("RedisMgr::Set, Failed : [SET {} {} ]", key, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	// 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	Logger::Debug("RedisMgr::Set, Succeed : [SET {} {} ]", key, value);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::SetExp(const std::string &key, const std::string &value, int expire_seconds)
{
	// 执行redis命令行
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "SETEX %s %d %s", key.c_str(),
											expire_seconds,
											value.c_str());

	if (NULL == reply)
	{
		Logger::Debug("RedisMgr::SetExp, Failed : [SETEX {} {} {} ]", key, expire_seconds, value);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (!(reply->type == REDIS_REPLY_STATUS &&
		  (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		Logger::Debug("RedisMgr::SetExp, Failed : [SETEX {} {} {} ]", key, expire_seconds, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	freeReplyObject(reply);
	Logger::Debug("RedisMgr::SetExp, Succeed : [SETEX {} {} {} ]", key, expire_seconds, value);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		Logger::Debug("RedisMgr::LPush, Failed : [LPUSH {} {} ]", key, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0)
	{
		Logger::Debug("RedisMgr::LPush, Failed : [LPUSH {} {} ]", key, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	Logger::Debug("RedisMgr::LPush, Succeed : [LPUSH {} {} ]", key, value);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::LPop(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::LPop, Failed : [LPOP {} ]", key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		Logger::Debug("RedisMgr::LPop, Failed : [LPOP {} ]", key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	value = reply->str;
	Logger::Debug("RedisMgr::LPop, Succeed : [LPOP {} ]", key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::RPush(const std::string &key, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		Logger::Debug("RedisMgr::RPush, Failed : [RPUSH {} {} ]", key, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0)
	{
		Logger::Debug("RedisMgr::RPush, Failed : [RPUSH {} {} ]", key, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	Logger::Debug("RedisMgr::RPush, Succeed : [RPUSH {} {} ]", key, value);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}
bool RedisMgr::RPop(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::RPop, Failed : [RPOP {} ]", key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		Logger::Debug("RedisMgr::RPop, Failed : [RPOP {} ]", key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	value = reply->str;
	Logger::Debug("RedisMgr::RPop, Succeed : [RPOP {} ]", key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::HSet, Failed : [HSET {} {} {} ]", key, hkey, value);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		Logger::Debug("RedisMgr::HSet, Failed : [HSET {} {} {} ]", key, hkey, value);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	Logger::Debug("RedisMgr::HSet, Succeed : [HSET {} {} {} ]", key, hkey, value);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::HSet(const char *key, const char *hkey, const char *hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	const char *argv[4];
	size_t argvlen[4];
	argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;

	auto reply = (redisReply *)redisCommandArgv(connect, 4, argv, argvlen);
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::HSet, Failed : [HSET {} {} {} ]", key, hkey, hvalue);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		Logger::Debug("RedisMgr::HSet, Failed : [HSET {} {} {} ]", key, hkey, hvalue);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	Logger::Debug("RedisMgr::HSet, Succeed : [HSET {} {} {} ]", key, hkey, hvalue);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return "";
	}
	const char *argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();

	auto reply = (redisReply *)redisCommandArgv(connect, 3, argv, argvlen);
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::HGet, Failed : [HGET {} {} ]", key, hkey);
		_con_pool->returnConnection(connect);
		return "";
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		freeReplyObject(reply);
		Logger::Debug("RedisMgr::HGet, Failed : [HGET {} {} ]", key, hkey);
		_con_pool->returnConnection(connect);
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	Logger::Debug("RedisMgr::HGet, Succeed : [HGET {} {} ]", key, hkey);
	return value;
}

bool RedisMgr::HDel(const std::string &key, const std::string &field)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}

	Defer defer([&connect, this]()
				{ _con_pool->returnConnection(connect); });

	redisReply *reply = (redisReply *)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::HDel, Failed : [HDEL {} {} ]", key, field);
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER)
	{
		success = reply->integer > 0;
	}

	freeReplyObject(reply);
	Logger::Debug("RedisMgr::HDel, {} : [HDEL {} {} ]", success ? "Succeed" : "Failed", key, field);
	return success;
}

bool RedisMgr::Del(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::Del, Failed : [DEL {} ]", key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		Logger::Debug("RedisMgr::Del, Failed : [DEL {} ]", key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	Logger::Debug("RedisMgr::Del, Succeed : [DEL {} ]", key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::ExistsKey(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}

	auto reply = (redisReply *)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr)
	{
		Logger::Debug("RedisMgr::ExistsKey, Failed : [EXISTS {} ]", key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0)
	{
		Logger::Debug("RedisMgr::ExistsKey, Failed : [EXISTS {} ]", key);
		_con_pool->returnConnection(connect);
		freeReplyObject(reply);
		return false;
	}
	Logger::Debug("RedisMgr::ExistsKey, Succeed : [EXISTS {} ]", key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

std::string RedisMgr::acquireLock(const std::string &lockName,
								  int lockTimeout, int acquireTimeout)
{

	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return "";
	}

	Defer defer([&connect, this]()
				{ _con_pool->returnConnection(connect); });

	Logger::Debug("RedisMgr::acquireLock, Succeed : lockName = {}", lockName);
	return DistLock::Inst().acquireLock(connect, lockName, lockTimeout, acquireTimeout);
}

bool RedisMgr::releaseLock(const std::string &lockName,
						   const std::string &identifier)
{
	if (identifier.empty())
	{
		return true;
	}
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}

	Defer defer([&connect, this]()
				{ _con_pool->returnConnection(connect); });

	Logger::Debug("RedisMgr::releaseLock, Succeed : lockName = {}", lockName);
	return DistLock::Inst().releaseLock(connect, lockName, identifier);
}

void RedisMgr::IncreaseCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	// 利用defer解锁
	Defer defer2([this, identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	// 将登录数量增加
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty())
	{
		count = std::stoi(rd_res);
	}

	count++;
	auto count_str = std::to_string(count);
	Logger::Debug("RedisMgr::IncreaseCount, server_name = {}, count = {}", server_name, count_str);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);
}

void RedisMgr::DecreaseCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	// 利用defer解锁
	Defer defer2([this, identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	// 将登录数量减少
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty())
	{
		count = std::stoi(rd_res);
		if (count > 0)
		{
			count--;
		}
	}

	auto count_str = std::to_string(count);
	Logger::Debug("RedisMgr::DecreaseCount, server_name = {}, count = {}", server_name, count_str);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);
}

void RedisMgr::InitCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	// 利用defer解锁
	Defer defer2([this, identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	Logger::Debug("RedisMgr::InitCount, server_name = {}, count = 0", server_name);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");
}

void RedisMgr::DelCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	// 利用defer解锁
	Defer defer2([this, identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	Logger::Debug("RedisMgr::DelCount, server_name = {}", server_name);
	RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
}

bool RedisMgr::SetFileInfo(const std::string &name, std::shared_ptr<FileInfo> file_info)
{
	Json::Reader reader;
	Json::Value root;
	root["file_path_str"] = file_info->_file_path_str;
	root["name"] = file_info->_name;
	root["seq"] = file_info->_seq;
	root["total_size"] = file_info->_total_size;
	root["trans_size"] = file_info->_trans_size;
	auto file_info_str = root.toStyledString();
	auto redis_key = "file_upload_" + name;
	bool success = SetExp(redis_key, file_info_str, 3600);
	Logger::Debug("RedisMgr::SetFileInfo, {} : [SETEX {} {} ]", success ? "Succeed" : "Failed", redis_key, file_info_str);
	return success;
}

bool RedisMgr::SetDownLoadInfo(const std::string &name, std::shared_ptr<FileInfo> file_info)
{
	Json::Reader reader;
	Json::Value root;
	root["file_path_str"] = file_info->_file_path_str;
	root["name"] = file_info->_name;
	root["seq"] = file_info->_seq;
	root["total_size"] = file_info->_total_size;
	root["trans_size"] = file_info->_trans_size;
	auto file_info_str = root.toStyledString();
	auto redis_key = "file_download_" + name;
	bool success = SetExp(redis_key, file_info_str, 3600);
	Logger::Debug("RedisMgr::SetDownLoadInfo, {} : [SETEX {} {} ]", success ? "Succeed" : "Failed", redis_key, file_info_str);
	return success;
}

bool RedisMgr::DelDownLoadInfo(const std::string &name)
{
	auto redis_key = "file_download_" + name;
	Logger::Debug("RedisMgr::DelDownLoadInfo, key = {}", redis_key);

	return Del(redis_key);
}

std::shared_ptr<FileInfo> RedisMgr::GetFileInfo(const std::string &name)
{
	auto redis_key = "file_upload_" + name;
	std::string file_info_str = "";

	// 从 Redis 获取数据
	bool success = Get(redis_key, file_info_str);
	if (!success || file_info_str.empty())
	{
		return nullptr;
	}

	// 解析 JSON
	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(file_info_str, root))
	{
		Logger::Debug("RedisMgr::GetFileInfo, Failed to parse file info JSON for name: {}", name);
		return nullptr;
	}

	// 创建 FileInfo 对象并填充数据
	auto file_info = std::make_shared<FileInfo>();
	try
	{
		file_info->_file_path_str = root["file_path_str"].asString();
		file_info->_name = root["name"].asString();
		file_info->_seq = root["seq"].asInt();
		file_info->_total_size = root["total_size"].asInt();
		file_info->_trans_size = root["trans_size"].asInt();
	}
	catch (const std::exception &e)
	{
		Logger::Error("RedisMgr::GetFileInfo, Error parsing file info fields for name: {}", name);
		return nullptr;
	}

	return file_info;
}

std::shared_ptr<FileInfo> RedisMgr::GetDownloadInfo(const std::string &name)
{
	auto redis_key = "file_download_" + name;
	std::string file_info_str = "";

	// 从 Redis 获取数据
	bool success = Get(redis_key, file_info_str);
	if (!success || file_info_str.empty())
	{
		return nullptr;
	}

	// 解析 JSON
	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(file_info_str, root))
	{
		Logger::Debug("RedisMgr::GetDownloadInfo, Failed to parse file info JSON for name: {}", name);

		return nullptr;
	}

	// 创建 FileInfo 对象并填充数据
	auto file_info = std::make_shared<FileInfo>();
	try
	{
		file_info->_file_path_str = root["file_path_str"].asString();
		file_info->_name = root["name"].asString();
		file_info->_seq = root["seq"].asInt();
		file_info->_total_size = root["total_size"].asInt();
		file_info->_trans_size = root["trans_size"].asInt();
	}
	catch (const std::exception &e)
	{
		Logger::Error("RedisMgr::GetDownloadInfo, Error parsing file info fields for name: {}", name);
		return nullptr;
	}

	return file_info;
}
