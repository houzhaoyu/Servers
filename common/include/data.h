#pragma once
#include <string>
#include "type.h"

struct UserInfo {
	UserInfo():name(""), pwd(""),uid(0),email(""),nick(""),desc(""),sex(0), icon(""), back("") {}
	std::string name;
	std::string pwd;
	UserIdType uid;
	std::string email;
	std::string nick;
	std::string desc;
	int sex;
	std::string icon;
	std::string back;
};

struct ApplyInfo {
	ApplyInfo(UserIdType uid, std::string name, std::string desc,
		std::string icon, std::string nick, int sex, int status)
		:_uid(uid),_name(name),_desc(desc),
		_icon(icon),_nick(nick),_sex(sex),_status(status){}

	UserIdType _uid;
	std::string _name;
	std::string _desc;
	std::string _icon;
	std::string _nick;
	int _sex;
	int _status;
};

//聊天线程信息
struct ChatThreadInfo {
	int _thread_id;
	std::string _type;     // "private" or "group"
	UserIdType _user1_id;    // 私聊时对应 private_chat.user1_id；群聊时设为 0
	UserIdType _user2_id;    // 私聊时对应 private_chat.user2_id；群聊时设为 0
};

//聊天消息信息
struct ChatMessage {
	int message_id;
	int thread_id;
	UserIdType sender_id;
	UserIdType recv_id;
	std::string unique_id;
	std::string content;
	std::string chat_time;
	int status;
	int msg_type;
};

// 查询结果结构，增加next_cursor字段
struct PageResult {
	std::vector<ChatMessage> messages;
	bool load_more;
	int next_cursor;  // 本页最后一条message_id，用于下次查询
};

enum class ChatMsgType {
	TEXT = 0,
	PIC = 1,
	VIDEO = 2,
	FILE = 3
};
