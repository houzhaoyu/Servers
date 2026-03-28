#include "BaseLogic.h"
#include "Singleton.h"
#include "ChatSession.h"
#include "CServer.h"
#include "data.h"

class LogicSystem : public BaseLogic, public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;
public:
	void SetServer(std::shared_ptr<CServer> pserver);

protected:
    // 实现基类的注册接口
	void RegisterHandlers() override;

private:
	LogicSystem() : BaseLogic(1) { RegisterHandlers(); Start(); }

	// 只需要保留业务相关的函数声明
	void LoginHandler(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void SearchInfo(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void AddFriendApply(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void AuthFriendApply(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void DealChatTextMsg(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void DealChatImgMsg(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void HeartBeatHandler(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	bool isPureDigit(const std::string& str);
	void GetUserByUid(std::string uid_str, Json::Value& rtvalue);
	void GetUserByName(std::string name, Json::Value& rtvalue);
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list);
	void GetUserThreadsHandler(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	bool GetUserThreads(int64_t userId,
		int64_t lastId,
		int      pageSize,
		std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
		bool& loadMore,
		int& nextLastId);
	void CreatePrivateChat(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);
	void LoadChatMsg(std::shared_ptr<ChatSession> session, const short& msg_id, const std::string& msg_data);

	std::shared_ptr<CServer> _p_server;
};
