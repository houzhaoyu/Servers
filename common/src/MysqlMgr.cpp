#include "MysqlMgr.h"
#include "Logger.h"

MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd, const std::string& icon)
{
	Logger::Debug("Mysql::RegUser: name = {}, email = {}", name, email);
	return _dao.RegUserTransaction(name, email, pwd, icon);
}

bool MysqlMgr::CheckEmail(const std::string &name, const std::string &email)
{
	Logger::Debug("Mysql::CheckEmail: name = {}, email = {}", name, email);
	return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string &name, const std::string &pwd)
{
	Logger::Debug("Mysql::UpdatePwd: name = {}", name);
	return _dao.UpdatePwd(name, pwd);
}

MysqlMgr::MysqlMgr()
{
}

bool MysqlMgr::CheckPwd(const std::string & email, const std::string &pwd, UserInfo &userInfo)
{
	Logger::Debug("Mysql::CheckPwd: email = {}", email);
	return _dao.CheckPwd(email, pwd, userInfo);
}

bool MysqlMgr::TestProcedure(const std::string &email, int &uid, std::string &name)
{
	Logger::Debug("Mysql::TestProcedure: name = {}, uid = {}", name, uid);
	return _dao.TestProcedure(email, uid, name);
}

bool MysqlMgr::AddFriendApply(const int &from, const int &to, const std::string &desc, const std::string &back_name)
{
	Logger::Debug("Mysql::AddFriendApply: from = {}, to = {}", from, to);
	return _dao.AddFriendApply(from, to, desc, back_name);
}

bool MysqlMgr::AuthFriendApply(const int &from, const int &to)
{
	Logger::Debug("Mysql::AuthFriendApply: from = {}, to = {}", from, to);
	return _dao.AuthFriendApply(from, to);
}

bool MysqlMgr::AddFriend(const int &from, const int &to, std::string back_name,
						 std::vector<std::shared_ptr<AddFriendMsg>> &msg_list)
{
	Logger::Debug("Mysql::AddFriend: from = {}, to = {}", from, to);
	return _dao.AddFriend(from, to, back_name, msg_list);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
	Logger::Debug("Mysql::GetUser: uid = {}", uid);
	return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(std::string name)
{
	Logger::Debug("Mysql::GetUser: name = {}", name);
	return _dao.GetUser(name);
}

bool MysqlMgr::GetApplyList(int touid,
							std::vector<std::shared_ptr<ApplyInfo>> &applyList, int begin, int limit)
{
	Logger::Debug("Mysql::GetApplyList: touid = {}", touid);
	return _dao.GetApplyList(touid, applyList, begin, limit);
}

bool MysqlMgr::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info)
{
	Logger::Debug("Mysql::GetFriendList: uid = {}", self_id);
	return _dao.GetFriendList(self_id, user_info);
}

bool MysqlMgr::GetUserThreads(int64_t userId,
							  int64_t lastId,
							  int pageSize,
							  std::vector<std::shared_ptr<ChatThreadInfo>> &threads,
							  bool &loadMore,
							  int &nextLastId)
{
	Logger::Debug("Mysql::GetUserThreads: uid = {}, lastId = {}, loadMore = {}", userId, lastId, loadMore);
	return _dao.GetUserThreads(userId, lastId, pageSize, threads, loadMore, nextLastId);
}

bool MysqlMgr::CreatePrivateChat(int user1_id, int user2_id, int &thread_id)
{
	Logger::Debug("Mysql::CreatePrivateChat: user1_id = {}, user2_id = {}", user1_id, user2_id);
	return _dao.CreatePrivateChat(user1_id, user2_id, thread_id);
}

std::shared_ptr<PageResult> MysqlMgr::LoadChatMsg(int threadId, int lastId, int pageSize)
{
	Logger::Debug("Mysql::LoadChatMsg: threadId = {}, lastId = {}", threadId, lastId);
	return _dao.LoadChatMsg(threadId, lastId, pageSize);
}

bool MysqlMgr::AddChatMsg(std::vector<std::shared_ptr<ChatMessage>> &chat_datas)
{
	Logger::Debug("Mysql::AddChatMsg");
	return _dao.AddChatMsg(chat_datas);
}

bool MysqlMgr::AddChatMsg(std::shared_ptr<ChatMessage> chat_data)
{
	Logger::Debug("Mysql::AddChatMsg");
	return _dao.AddChatMsg(chat_data);
}

bool MysqlMgr::UpdateUserIcon(int uid, const std::string& icon) {
	Logger::Debug("Mysql::UpdateUserIcon, uid = {}", uid);
	return _dao.UpdateHeadInfo(uid, icon);
}

bool MysqlMgr::UpdateUploadStatus(int chat_messag_id)
{
	return _dao.UpdateUploadStatus(chat_messag_id);
}

std::shared_ptr<ChatImgInfo> MysqlMgr::GetImgInfoByMsgId(int msg_id)
{
	//查询数据库获取图片信息
	return _dao.GetImgInfoByMsgId(msg_id);
}

std::shared_ptr<ChatMessage> MysqlMgr::GetChatMsgById(int message_id)
{
	return _dao.GetChatMsgById(message_id);
}
