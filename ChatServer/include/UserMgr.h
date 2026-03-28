#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class ChatSession;
class UserMgr: public Singleton<UserMgr>
{
	friend class Singleton<UserMgr>;
public:
	~UserMgr();
	std::shared_ptr<ChatSession> GetSession(int uid);
	void SetUserSession(int uid, std::shared_ptr<ChatSession> session);
	void RmvUserSession(int uid, std::string session_id);
private:
	UserMgr();
	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<ChatSession>> _uid_to_session;
};


