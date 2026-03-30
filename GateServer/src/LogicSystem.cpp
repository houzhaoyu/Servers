#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VerifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
#include "Logger.h"

LogicSystem::LogicSystem()
{
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
				std::cout << "receive body is " << body_str << std::endl;
				connection->_response.set(http::field::content_type, "text/json");
				Json::Value root;
				Json::Reader reader;
				Json::Value src_root;
				bool parse_success = reader.parse(body_str, src_root);
				if (!parse_success)
				{
					std::cout << "Failed to parse JSON data!" << std::endl;
					root["error"] = ErrorCodes::Error_Json;
					std::string jsonstr = root.toStyledString();
					beast::ostream(connection->_response.body()) << jsonstr;
					return true;
				}

				if (!src_root.isMember("email"))
				{
					std::cout << "Failed to parse JSON data!" << std::endl;
					root["error"] = ErrorCodes::Error_Json;
					std::string jsonstr = root.toStyledString();
					beast::ostream(connection->_response.body()) << jsonstr;
					return true;
				}

				auto email = src_root["email"].asString();
				int uid = 0;
				std::string name = "";
				MysqlMgr::GetInstance()->TestProcedure(email, uid, name);
				std::cout << "email is " << email << std::endl;
				root["error"] = ErrorCodes::Success;
				root["email"] = src_root["email"];
				root["name"] = name;
				root["uid"] = uid;
				std::string jsonstr = root.toStyledString();
				beast::ostream(connection->_response.body()) << jsonstr;
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
		GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
		root["error"] = rsp.error();
		root["email"] = src_root["email"];
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
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

		//先查找redis中email对应的验证码是否合理
		std::string  Verify_code;
		bool b_get_Verify = RedisMgr::GetInstance()->Get(CODE_PREFIX + src_root["email"].asString(), Verify_code);
		if (!b_get_Verify) {
			Logger::Error("get Verify code expired");
			root["error"] = ErrorCodes::VerifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (Verify_code != src_root["Verifycode"].asString()) {
			Logger::Error("Verify code error");
			root["error"] = ErrorCodes::VerifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查找数据库判断用户是否存在
		int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd, icon);
		if (uid == 0 || uid == -1) {
			Logger::Error("user exist");
			root["error"] = ErrorCodes::UserExist;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		root["error"] = 0;
		root["uid"] = uid;
		root["email"] = email;
		root["user"] = name;
		root["passwd"] = pwd;
		root["confirm"] = confirm;
		root["icon"] = icon;
		root["Verifycode"] = src_root["Verifycode"].asString();
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
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

		//先查找redis中email对应的验证码是否合理
		std::string  Verify_code;
		bool b_get_Verify = RedisMgr::GetInstance()->Get(CODE_PREFIX + src_root["email"].asString(), Verify_code);
		if (!b_get_Verify) {
			Logger::Error("get Verify code expired");
			root["error"] = ErrorCodes::VerifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (Verify_code != src_root["Verifycode"].asString()) {
			Logger::Error("Verify code error");
			root["error"] = ErrorCodes::VerifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//查询数据库判断用户名和邮箱是否匹配
		bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
		if (!email_valid) {
			Logger::Error(" user email not match");
			root["error"] = ErrorCodes::EmailNotMatch;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//更新密码为最新密码
		bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
		if (!b_up) {
			Logger::Error("update password failed");
			root["error"] = ErrorCodes::PasswdUpFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		Logger::Info("succeed to update password, email is {}, user is {}", email, name);
		root["error"] = 0;
		root["email"] = email;
		root["user"] = name;
		root["passwd"] = pwd;
		root["Verifycode"] = src_root["Verifycode"].asString();
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true; });

	// 用户登录逻辑
	RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection)
			{
			Logger::Info("receive user_login req");
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		std::cout << "receive body is " << body_str << std::endl;
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
		UserInfo userInfo;
		//查询数据库判断用户名和密码是否匹配
		bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
		if (!pwd_valid) {
			Logger::Error(" user pwd not match");
			root["error"] = ErrorCodes::PasswdInvalid;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查询StatusServer找到合适的连接
		auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
		if (reply.error()) {
			Logger::Error("grpc get chat server failed, error is {}", reply.error());
			root["error"] = ErrorCodes::RPCFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		Logger::Info("succeed to login, email is {}, uid is {}, chat host is {}, chat port is {}", email, userInfo.uid, reply.host(), reply.port());
		root["error"] = 0;
		root["email"] = email;
		root["uid"] = userInfo.uid;
		root["token"] = reply.token();
		root["chathost"] = reply.host();
		root["chatport"] = reply.port();
		auto& gCfgMgr = ConfigMgr::Inst();
		std::string res_port = gCfgMgr["ResourceServer"]["Port"];
		std::string res_host = gCfgMgr["ResourceServer"]["Host"];
		root["reshost"] = res_host;
		root["resport"] = res_port;

		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true; });
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
