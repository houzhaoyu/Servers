#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VerifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
#include "Logger.h"
#include "ConfigMgr.h"
#include <boost/asio/post.hpp>
#include <algorithm>
#include <thread>

namespace
{
	std::size_t GetBusinessThreadCount()
	{
		auto value = ConfigMgr::Inst()["GateServer"]["BusinessThreads"];
		if (!value.empty())
		{
			try
			{
				return std::clamp<std::size_t>(std::stoul(value), 1, 64);
			}
			catch (const std::exception &)
			{
				Logger::Error("invalid GateServer.BusinessThreads: {}", value);
			}
		}
		return std::max<std::size_t>(8, std::thread::hardware_concurrency());
	}
}

LogicSystem::LogicSystem() : _business_pool(GetBusinessThreadCount())
{
	Logger::Info("GateServer business thread pool started, size = {}", GetBusinessThreadCount());
	RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection)
		   {
		beast::ostream(connection->_response.body()) << "receive get_test req " << std::endl;
		int i = 0;
		for (auto& elem : connection->_get_params) {
			i++;
			beast::ostream(connection->_response.body()) << "param" << i << " key is " << elem.first;
			beast::ostream(connection->_response.body()) << ", " << " value is " << elem.second << std::endl;
		}

		connection->_response.set(http::field::content_type, "text/plain"); });

	RegPost("/test_procedure", [](std::shared_ptr<HttpConnection> connection)
			{
				auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
				// Logger::Info("receive body is {}", body_str);
				connection->_response.set(http::field::content_type, "text/json");
				Json::Value root;
				Json::Reader reader;
				Json::Value src_root;
				bool parse_success = reader.parse(body_str, src_root);
				if (!parse_success)
				{
					Logger::Error("Failed to parse JSON data!");
					root["error"] = ErrorCodes::Error_Json;
					std::string jsonstr = root.toStyledString();
					beast::ostream(connection->_response.body()) << jsonstr;
					return true;
				}

				if (!src_root.isMember("email"))
				{
					Logger::Error("Failed to parse JSON data!");
					root["error"] = ErrorCodes::Error_Json;
					std::string jsonstr = root.toStyledString();
					beast::ostream(connection->_response.body()) << jsonstr;
					return true;
				}

				auto email = src_root["email"].asString();
				LogicSystem::GetInstance()->PostJsonTask(connection, [email]()
					{
						UserIdType uid = 0;
						std::string name;
						MysqlMgr::GetInstance()->TestProcedure(email, uid, name);
						Json::Value result;
						result["error"] = ErrorCodes::Success;
						result["email"] = email;
						result["name"] = name;
						result["uid"] = uid;
						return result.toStyledString();
					});
				return true; });

	RegPost("/get_Verifycode", [](std::shared_ptr<HttpConnection> connection)
			{
				Logger::Info("receive get_Verifycode req");
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			Logger::Error("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (!src_root.isMember("email")) {
			Logger::Error("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		connection->DeferResponse();
		VerifyGrpcClient::GetInstance()->AsyncGetVerifyCode(email,
			[connection, email](GetVerifyRsp rsp)
			{
				Json::Value result;
				result["error"] = rsp.error();
				result["email"] = email;
				connection->CompleteJsonResponse(result.toStyledString());
			});
		return true; });
	// day11 注册用户逻辑
	RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection)
			{
				Logger::Info("receive user_register req");
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			Logger::Error("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["passwd"].asString();
		auto confirm = src_root["confirm"].asString();
		auto icon = src_root["icon"].asString();

		if (pwd != confirm) {
			Logger::Error("password not match");
			root["error"] = ErrorCodes::PasswdErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto request_verify_code = src_root["Verifycode"].asString();
		connection->DeferResponse();
		RedisMgr::GetInstance()->AsyncGet(CODE_PREFIX + email,
			[connection, email, name, pwd, confirm, icon, request_verify_code](RedisAsyncResult redis_result)
			{
				Json::Value result;
				if (!redis_result.success)
				{
					Logger::Error("get Verify code expired");
					result["error"] = ErrorCodes::VerifyExpired;
					connection->CompleteJsonResponse(result.toStyledString());
					return;
				}
				if (redis_result.value != request_verify_code)
				{
					Logger::Error("Verify code error");
					result["error"] = ErrorCodes::VerifyCodeErr;
					connection->CompleteJsonResponse(result.toStyledString());
					return;
				}

				LogicSystem::GetInstance()->PostTask(
					[connection, email, name, pwd, confirm, icon, request_verify_code]()
					{
						Json::Value db_result;
						UserIdType uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd, icon);
						if (uid == 0 || uid == -1)
						{
							Logger::Error("user exist");
							db_result["error"] = ErrorCodes::UserExist;
							connection->CompleteJsonResponse(db_result.toStyledString());
							return;
						}
						db_result["error"] = 0;
						db_result["uid"] = uid;
						db_result["email"] = email;
						db_result["user"] = name;
						db_result["passwd"] = pwd;
						db_result["confirm"] = confirm;
						db_result["icon"] = icon;
						db_result["Verifycode"] = request_verify_code;
						connection->CompleteJsonResponse(db_result.toStyledString());
					});
			});
		return true; });

	// 重置回调逻辑
	RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection)
			{
			Logger::Info("receive reset_pwd req");
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			Logger::Error("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["passwd"].asString();
		auto request_verify_code = src_root["Verifycode"].asString();
		connection->DeferResponse();
		RedisMgr::GetInstance()->AsyncGet(CODE_PREFIX + email,
			[connection, email, name, pwd, request_verify_code](RedisAsyncResult redis_result)
			{
				Json::Value result;
				if (!redis_result.success)
				{
					Logger::Error("get Verify code expired");
					result["error"] = ErrorCodes::VerifyExpired;
					connection->CompleteJsonResponse(result.toStyledString());
					return;
				}
				if (redis_result.value != request_verify_code)
				{
					Logger::Error("Verify code error");
					result["error"] = ErrorCodes::VerifyCodeErr;
					connection->CompleteJsonResponse(result.toStyledString());
					return;
				}
				LogicSystem::GetInstance()->PostTask(
					[connection, email, name, pwd, request_verify_code]()
					{
						Json::Value db_result;
						if (!MysqlMgr::GetInstance()->CheckEmail(name, email))
						{
							db_result["error"] = ErrorCodes::EmailNotMatch;
							connection->CompleteJsonResponse(db_result.toStyledString());
							return;
						}
						if (!MysqlMgr::GetInstance()->UpdatePwd(name, pwd))
						{
							db_result["error"] = ErrorCodes::PasswdUpFailed;
							connection->CompleteJsonResponse(db_result.toStyledString());
							return;
						}
						db_result["error"] = 0;
						db_result["email"] = email;
						db_result["user"] = name;
						db_result["passwd"] = pwd;
						db_result["Verifycode"] = request_verify_code;
						connection->CompleteJsonResponse(db_result.toStyledString());
					});
			});
		return true; });

	// 用户登录逻辑
	RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection)
			{
			Logger::Info("receive user_login req");
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		// std::cout << "receive body is " << body_str << std::endl;
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			Logger::Error("Failed to parse JSON data!");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto pwd = src_root["passwd"].asString();
		connection->DeferResponse();
		LogicSystem::GetInstance()->PostTask([connection, email, pwd]()
			{
				Json::Value result;
				UserInfo userInfo;
				if (!MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo))
				{
					Logger::Error("user pwd not match");
					result["error"] = ErrorCodes::PasswdInvalid;
					connection->CompleteJsonResponse(result.toStyledString());
					return;
				}

				StatusGrpcClient::GetInstance()->AsyncGetChatServer(userInfo.uid,
					[connection, email, userInfo = std::move(userInfo)](GetChatServerRsp reply)
					{
						Json::Value rpc_result;
						if (reply.error())
						{
							Logger::Error("grpc get chat server failed, error is {}", reply.error());
							rpc_result["error"] = ErrorCodes::RPCFailed;
							connection->CompleteJsonResponse(rpc_result.toStyledString());
							return;
						}

						Logger::Info("succeed to login, email is {}, uid is {}, chat host is {}, chat port is {}",
							email, userInfo.uid, reply.host(), reply.port());
						rpc_result["error"] = 0;
						rpc_result["email"] = email;
						rpc_result["uid"] = userInfo.uid;
						rpc_result["token"] = reply.token();
						rpc_result["chathost"] = reply.host();
						rpc_result["chatport"] = reply.port();
						auto &cfg = ConfigMgr::Inst();
						rpc_result["reshost"] = cfg["ResourceServer"]["Host"];
						rpc_result["resport"] = cfg["ResourceServer"]["Port"];
						connection->CompleteJsonResponse(rpc_result.toStyledString());
					});
			});
		return true; });
}

void LogicSystem::PostTask(std::function<void()> task)
{
	boost::asio::post(_business_pool,
		[task = std::move(task)]() mutable
		{
			try
			{
				task();
			}
			catch (const std::exception &error)
			{
				Logger::Error("GateServer async business task failed: {}", error.what());
			}
		});
}

void LogicSystem::PostJsonTask(std::shared_ptr<HttpConnection> connection,
	std::function<std::string()> task)
{
	connection->DeferResponse();
	boost::asio::post(_business_pool,
		[connection, task = std::move(task)]() mutable
		{
			try
			{
				connection->CompleteJsonResponse(task());
			}
			catch (const std::exception &error)
			{
				Logger::Error("GateServer async business task failed: {}", error.what());
				Json::Value result;
				result["error"] = ErrorCodes::RPCFailed;
				connection->CompleteJsonResponse(result.toStyledString(),
					http::status::internal_server_error);
			}
		});
}

void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
	_get_handlers.insert(make_pair(url, handler));
}

void LogicSystem::RegPost(std::string url, HttpHandler handler)
{
	_post_handlers.insert(make_pair(url, handler));
}

LogicSystem::~LogicSystem()
{
	_business_pool.join();
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con)
{
	if (_get_handlers.find(path) == _get_handlers.end())
	{
		return false;
	}

	_get_handlers[path](con);
	return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con)
{
	if (_post_handlers.find(path) == _post_handlers.end())
	{
		return false;
	}

	_post_handlers[path](con);
	return true;
}
