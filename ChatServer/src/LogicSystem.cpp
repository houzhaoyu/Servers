#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
#include "utils.h"
#include "data.h"
#include "Logger.h"

void LogicSystem::SetServer(std::shared_ptr<CServer> pserver)
{
	_p_server = pserver;
}

void LogicSystem::RegisterHandlers()
{
	_handlers[ID_CHAT_LOGIN] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		LoginHandler(s, msg_id, msg_data);
	};

	_handlers[ID_SEARCH_USER_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });

		SearchInfo(s, msg_id, msg_data);
	};

	_handlers[ID_ADD_FRIEND_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });

		AddFriendApply(s, msg_id, msg_data);
	};

	_handlers[ID_AUTH_FRIEND_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		AuthFriendApply(s, msg_id, msg_data);
	};

	_handlers[ID_TEXT_CHAT_MSG_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		DealChatTextMsg(s, msg_id, msg_data);
	};

	_handlers[ID_HEART_BEAT_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		HeartBeatHandler(s, msg_id, msg_data);
	};

	_handlers[ID_LOAD_CHAT_THREAD_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		GetUserThreadsHandler(s, msg_id, msg_data);
	};

	_handlers[ID_CREATE_PRIVATE_CHAT_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		CreatePrivateChat(s, msg_id, msg_data);
	};

	_handlers[ID_LOAD_CHAT_MSG_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		LoadChatMsg(s, msg_id, msg_data);
	};

	_handlers[ID_IMG_CHAT_MSG_REQ] =
		[this](std::shared_ptr<BaseSession> session, MsgIdType msg_id, const std::string &msg_data)
	{
		auto s = std::dynamic_pointer_cast<ChatSession>(session);
		if (!s)
			return;

		std::string trace_id = std::to_string(s->GetUserId());
		LogContext::SetTraceId(trace_id);
		Defer defer([]()
					{ LogContext::Clear(); });
		DealChatImgMsg(s, msg_id, msg_data);
	};
}

void LogicSystem::LoginHandler(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	Logger::Debug("LogicSystem::LoginHandler - user login uid is {}", uid);

	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CHAT_LOGIN_RSP); });

	// 从redis获取用户token是否正确
	std::string uid_str = std::to_string(uid);
	std::string token_key = USER_TOKEN_PREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	if (token_value != token)
	{
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}

	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	rtvalue["token"] = token;

	// 从数据库获取申请列表
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply)
	{
		for (auto &apply : apply_list)
		{
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	// 获取好友列表
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto &friend_ele : friend_list)
	{
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["nick"] = friend_ele->nick;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		rtvalue["friend_list"].append(obj);
	}

	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	{
		// 此处添加分布式锁，让该线程独占登录
		// 拼接用户ip对应的key
		auto lock_key = LOCK_PREFIX + uid_str;
		auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
		// 利用defer解锁
		Defer defer2([this, identifier, lock_key]()
					 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });
		// 此处判断该用户是否在别处或者本服务器登录

		std::string uid_ip_value = "";
		auto uid_ip_key = USER_IP_PREFIX + uid_str;
		bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
		// 说明用户已经登录了，此处应该踢掉之前的用户登录状态
		if (b_ip)
		{
			// 获取当前服务器ip信息
			auto &cfg = ConfigMgr::Inst();
			auto self_name = cfg["SelfServer"]["Name"];
			// 如果之前登录的服务器和当前相同，则直接在本服务器踢掉
			if (uid_ip_value == self_name)
			{
				// 查找旧有的连接
				auto old_session = UserMgr::GetInstance()->GetSession(uid);

				// 此处应该发送踢人消息
				if (old_session)
				{
					old_session->NotifyOffline(uid);
					// 清除旧的连接
					_p_server->RemoveSession(old_session->GetSessionId());
				}
			}
			else
			{
				// 如果不是本服务器，则通知grpc通知其他服务器踢掉
				// 发送通知
				KickUserReq kick_req;
				kick_req.set_uid(uid);
				ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
			}
		}

		// session绑定用户uid
		session->SetUserId(uid);
		// 为用户设置登录ip server的名字
		std::string ipkey = USER_IP_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(ipkey, server_name);
		// uid和session绑定管理,方便以后踢人操作
		UserMgr::GetInstance()->SetUserSession(uid, session);
		std::string uid_session_key = USER_SESSION_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());
	}

	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	Logger::Debug("LogicSystem::SearchInfo - user SearchInfo uid is {}", uid_str);

	Json::Value rtvalue;

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP); });

	bool b_digit = isPureDigit(uid_str);
	if (b_digit)
	{
		GetUserByUid(uid_str, rtvalue);
	}
	else
	{
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

void LogicSystem::AddFriendApply(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto desc = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto touid = root["touid"].asInt();

	Logger::Debug("LogicSystem::AddFriendApply - user AddFriendApply uid is {}", uid);
	Logger::Debug("LogicSystem::AddFriendApply - applydesc is {}", desc);
	Logger::Debug("LogicSystem::AddFriendApply - bakname is {}", bakname);
	Logger::Debug("LogicSystem::AddFriendApply - touid is {}", touid);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP); });

	// 先更新数据库
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid, desc, bakname);

	// 查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USER_IP_PREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		return;
	}

	auto &cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	// 直接通知对方有申请消息
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session)
		{
			// 在内存中则直接发送通知对方
			Json::Value notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = apply_info->name;
			notify["desc"] = desc;
			if (b_info)
			{
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
		}

		return;
	}

	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(apply_info->name);
	add_req.set_desc(desc);
	if (b_info)
	{
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}

	// 发送通知
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void LogicSystem::AuthFriendApply(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{

	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto back_name = root["back"].asString();
	Logger::Debug("LogicSystem::AuthFriendApply - user fromuid is {} auth friend touid is {}", uid, touid);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info)
	{
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
	}
	else
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP); });

	// 先更新数据库， 放到事务中，此处不再处理
	// MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	std::vector<std::shared_ptr<AddFriendMsg>> chat_datas;

	// 更新数据库添加好友
	MysqlMgr::GetInstance()->AddFriend(uid, touid, back_name, chat_datas);

	// 查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USER_IP_PREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		return;
	}

	auto &cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	// 直接通知对方有认证通过消息
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session)
		{
			// 在内存中则直接发送通知对方
			Json::Value notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info)
			{
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else
			{
				notify["error"] = ErrorCodes::UidInvalid;
			}

			auto chat_time = getCurrentTimestamp();
			for (auto &chat_data : chat_datas)
			{
				Json::Value chat;
				chat["sender"] = chat_data->sender_id();
				chat["msg_id"] = chat_data->msg_id();
				chat["thread_id"] = chat_data->thread_id();
				chat["unique_id"] = chat_data->unique_id();
				chat["msg_content"] = chat_data->msgcontent();
				chat["chat_time"] = chat_time;
				chat["status"] = chat_data->status();
				notify["chat_datas"].append(chat);
				rtvalue["chat_datas"].append(chat);
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}

		return;
	}

	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);
	auto chat_time = getCurrentTimestamp();
	for (auto &chat_data : chat_datas)
	{
		auto text_msg = auth_req.add_textmsgs();
		text_msg->CopyFrom(*chat_data);
		Json::Value chat;
		chat["sender"] = chat_data->sender_id();
		chat["msg_id"] = chat_data->msg_id();
		chat["thread_id"] = chat_data->thread_id();
		chat["unique_id"] = chat_data->unique_id();
		chat["msg_content"] = chat_data->msgcontent();
		chat["chat_time"] = chat_time;
		chat["status"] = chat_data->status();
		rtvalue["chat_datas"].append(chat);
	}
	// 发送通知
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	Logger::Debug("LogicSystem::DealChatTextMsg - receive text chat msg, msg data is {}", msg_data);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	const Json::Value arrays = root["text_array"];

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;
	std::vector<std::shared_ptr<ChatMessage>> chat_datas;
	auto timestamp = getCurrentTimestamp();
	for (const auto &txt_obj : arrays)
	{
		auto content = txt_obj["content"].asString();
		auto unique_id = txt_obj["unique_id"].asString();
		auto chat_msg = std::make_shared<ChatMessage>();
		chat_msg->chat_time = timestamp;
		chat_msg->sender_id = uid;
		chat_msg->recv_id = touid;
		chat_msg->unique_id = unique_id;
		chat_msg->thread_id = thread_id;
		chat_msg->content = content;
		chat_msg->status = 2;
		chat_datas.push_back(chat_msg);
	}

	// 插入数据库
	MysqlMgr::GetInstance()->AddChatMsg(chat_datas);

	for (const auto &chat_data : chat_datas)
	{
		Json::Value chat_msg;
		chat_msg["message_id"] = chat_data->message_id;
		chat_msg["unique_id"] = chat_data->unique_id;
		chat_msg["content"] = chat_data->content;
		chat_msg["status"] = chat_data->status;
		chat_msg["chat_time"] = chat_data->chat_time;
		rtvalue["chat_datas"].append(chat_msg);
	}

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_TEXT_CHAT_MSG_RSP); });

	// 查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USER_IP_PREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		return;
	}

	auto &cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	// 直接通知对方有认证通过消息
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session)
		{
			// 在内存中则直接发送通知对方
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
		}

		return;
	}

	TextChatMsgReq text_msg_req;
	text_msg_req.set_fromuid(uid);
	text_msg_req.set_touid(touid);
	text_msg_req.set_thread_id(thread_id);
	for (const auto &chat_data : chat_datas)
	{
		auto *text_msg = text_msg_req.add_textmsgs();
		text_msg->set_unique_id(chat_data->unique_id);
		text_msg->set_msgcontent(chat_data->content);
		text_msg->set_msg_id(chat_data->message_id);
		text_msg->set_chat_time(chat_data->chat_time);
	}

	// 发送通知 todo...
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::DealChatImgMsg(std::shared_ptr<ChatSession> session,
								 const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	Logger::Debug("LogicSystem::DealChatImgMsg - receive img chat msg");

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	auto md5 = root["md5"].asString();
	auto unique_name = root["name"].asString();
	auto token = root["token"].asString();
	auto unique_id = root["unique_id"].asString();
	auto chat_time = root["chat_time"].asString();
	auto status = root["status"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;
	rtvalue["md5"] = md5;
	rtvalue["unique_name"] = unique_name;
	rtvalue["unique_id"] = unique_id;
	rtvalue["chat_time"] = chat_time;
	rtvalue["status"] = MsgStatus::UN_UPLOAD;

	auto timestamp = getCurrentTimestamp();
	auto chat_msg = std::make_shared<ChatMessage>();
	chat_msg->chat_time = timestamp;
	chat_msg->sender_id = uid;
	chat_msg->recv_id = touid;
	chat_msg->unique_id = unique_id;
	chat_msg->thread_id = thread_id;
	chat_msg->content = unique_name;
	chat_msg->status = MsgStatus::UN_UPLOAD;
	chat_msg->msg_type = int(ChatMsgType::PIC);

	// 插入数据库
	MysqlMgr::GetInstance()->AddChatMsg(chat_msg);

	rtvalue["message_id"] = chat_msg->message_id;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_IMG_CHAT_MSG_RSP); });
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	Logger::Debug("LogicSystem::HeartBeatHandler - receive heart beat msg, uid is {}", uid);
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::isPureDigit(const std::string &str)
{
	for (char c : str)
	{
		if (!std::isdigit(c))
		{
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value &rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;
	Logger::Debug("LogicSystem::GetUserByUid - GetUserByUid uid is {}", uid_str);

	std::string base_key = USER_BASE_INFO + uid_str;

	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	// redis中没有则查询mysql
	// 查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	// 将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	// 返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

void LogicSystem::GetUserByName(std::string name, Json::Value &rtvalue)
{
	Logger::Debug("LogicSystem::GetUserByName - GetUserByName name is {}", name);
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	// redis中没有则查询mysql
	// 查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	// 将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	// 返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

bool LogicSystem::GetBaseInfo(std::string base_key, UserIdType uid, std::shared_ptr<UserInfo> &userinfo)
{
	Logger::Debug("LogicSystem::GetBaseInfo - GetBaseInfo uid is {}", uid);
	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		Logger::Debug("LogicSystem::GetBaseInfo - GetBaseInfo from redis, uid is {}, name is {}, email is {}", userinfo->uid, userinfo->name, userinfo->email);
	}
	else
	{
		// redis中没有则查询mysql
		// 查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr)
		{
			return false;
		}

		userinfo = user_info;

		// 将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}

	return true;
}

bool LogicSystem::GetFriendApplyInfo(UserIdType to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list)
{
	Logger::Debug("LogicSystem::GetFriendApplyInfo - GetFriendApplyInfo to_uid is {}", to_uid);
	// 从mysql获取好友申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_list)
{
	Logger::Debug("LogicSystem::GetFriendList - GetFriendList self_id is {}", self_id);
	// 从mysql获取好友列表
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}

void LogicSystem::GetUserThreadsHandler(std::shared_ptr<ChatSession> session,
										const MsgIdType &msg_id, const std::string &msg_data)
{
	Logger::Debug("LogicSystem::GetUserThreadsHandler - GetUserThreadsHandler msg_id is {}", msg_id);

	// 从数据库加chat_threads记录
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	int last_id = root["thread_id"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_THREAD_RSP); });

	std::vector<std::shared_ptr<ChatThreadInfo>> threads;

	int page_size = 10;
	bool load_more = false;
	int next_last_id = 0;
	bool res = GetUserThreads(uid, last_id, page_size, threads, load_more, next_last_id);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	rtvalue["load_more"] = load_more;
	rtvalue["next_last_id"] = (int)next_last_id;
	// 整理threads数据写入json返回
	for (auto &thread : threads)
	{
		Json::Value thread_value;
		thread_value["thread_id"] = int(thread->_thread_id);
		thread_value["type"] = thread->_type;
		thread_value["user1_id"] = thread->_user1_id;
		thread_value["user2_id"] = thread->_user2_id;
		rtvalue["threads"].append(thread_value);
	}
}

bool LogicSystem::GetUserThreads(UserIdType userId,
									UserIdType lastId,
								 int pageSize,
								 std::vector<std::shared_ptr<ChatThreadInfo>> &threads,
								 bool &loadMore,
								 int &nextLastId)
{
	Logger::Debug("LogicSystem::GetUserThreads - GetUserThreads userId is {}, lastId is {}, pageSize is {}", userId, lastId, pageSize);

	return MysqlMgr::GetInstance()->GetUserThreads(userId, lastId, pageSize,
												   threads, loadMore, nextLastId);
}

void LogicSystem::CreatePrivateChat(std::shared_ptr<ChatSession> session, const MsgIdType &msg_id, const std::string &msg_data)
{
	Logger::Debug("LogicSystem::CreatePrivateChat - CreatePrivateChat msg Id is {}", msg_id);
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto other_id = root["other_id"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["other_id"] = other_id;

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_PRIVATE_CHAT_RSP); });

	int thread_id = 0;
	bool res = MysqlMgr::GetInstance()->CreatePrivateChat(uid, other_id, thread_id);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		return;
	}

	rtvalue["thread_id"] = thread_id;
}

void LogicSystem::LoadChatMsg(std::shared_ptr<ChatSession> session,
							  const MsgIdType &msg_id, const std::string &msg_data)
{
	Logger::Debug("LogicSystem::LoadChatMsg - LoadChatMsg msg Id is {}", msg_id);
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto thread_id = root["thread_id"].asInt();
	auto message_id = root["message_id"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_MSG_RSP); });

	int page_size = 10;
	std::shared_ptr<PageResult> res = MysqlMgr::GetInstance()->LoadChatMsg(thread_id, message_id, page_size);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::LOAD_CHAT_FAILED;
		return;
	}

	rtvalue["last_message_id"] = res->next_cursor;
	rtvalue["load_more"] = res->load_more;
	for (auto &chat : res->messages)
	{
		Json::Value chat_data;
		chat_data["sender"] = chat.sender_id;
		chat_data["msg_id"] = chat.message_id;
		chat_data["thread_id"] = chat.thread_id;
		chat_data["unique_id"] = 0;
		chat_data["msg_content"] = chat.content;
		chat_data["chat_time"] = chat.chat_time;
		chat_data["status"] = chat.status;
		chat_data["msg_type"] = chat.msg_type;
		chat_data["receiver"] = chat.recv_id;
		rtvalue["chat_datas"].append(chat_data);
	}
}
