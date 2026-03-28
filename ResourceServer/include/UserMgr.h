#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class FileSession;
class UserMgr: public Singleton<UserMgr>
{
	friend class Singleton<UserMgr>;
public:
	~UserMgr();
	std::shared_ptr<FileSession> GetSession(int uid);
	void SetUserSession(int uid, std::shared_ptr<FileSession> session);
	void RmvUserSession(int uid, std::string session_id);
	void RmvUserSession(int uid);
private:
	UserMgr();
	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<FileSession>> _uid_to_session;
};


