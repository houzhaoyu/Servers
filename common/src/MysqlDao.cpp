#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlDao::MysqlDao()
{
	auto &cfg = ConfigMgr::Inst();
	const auto &host = cfg["Mysql"]["Host"];
	const auto &port = cfg["Mysql"]["Port"];
	const auto &pwd = cfg["Mysql"]["Passwd"];
	const auto &schema = cfg["Mysql"]["Schema"];
	const auto &user = cfg["Mysql"]["User"];
	pool_.reset(new MySqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao()
{
	pool_->Close();
}

int MysqlDao::RegUser(const std::string &name, const std::string &email, const std::string &pwd)
{
	auto con = pool_->getConnection();
	try
	{
		if (con == nullptr)
		{
			return false;
		}

		std::unique_ptr<sql::PreparedStatement> stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));

		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		stmt->execute();

		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
		if (res->next())
		{
			int result = res->getInt("result");
			std::cout << "Result: " << result << std::endl;
			pool_->returnConnection(std::move(con));
			return result;
		}
		pool_->returnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException &e)
	{
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
}

int MysqlDao::RegUserTransaction(const std::string &name, const std::string &email, const std::string &pwd,
								 const std::string &icon)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		con->_con->setAutoCommit(false);

		std::unique_ptr<sql::PreparedStatement> pstmt_email(con->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));

		pstmt_email->setString(1, email);

		std::unique_ptr<sql::ResultSet> res_email(pstmt_email->executeQuery());

		auto email_exist = res_email->next();
		if (email_exist)
		{
			con->_con->rollback();
			std::cout << "email " << email << " exist";
			return 0;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt_name(con->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));

		pstmt_name->setString(1, name);

		std::unique_ptr<sql::ResultSet> res_name(pstmt_name->executeQuery());

		auto name_exist = res_name->next();
		if (name_exist)
		{
			con->_con->rollback();
			std::cout << "name " << name << " exist";
			return 0;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt_upid(con->_con->prepareStatement("UPDATE user_id SET id = id + 1"));

		pstmt_upid->executeUpdate();

		std::unique_ptr<sql::PreparedStatement> pstmt_uid(con->_con->prepareStatement("SELECT id FROM user_id"));
		std::unique_ptr<sql::ResultSet> res_uid(pstmt_uid->executeQuery());
		int newId = 0;
		if (res_uid->next())
		{
			newId = res_uid->getInt("id");
		}
		else
		{
			std::cout << "select id from user_id failed" << std::endl;
			con->_con->rollback();
			return -1;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt_insert(con->_con->prepareStatement("INSERT INTO user (uid, name, email, pwd) "
																						 "VALUES (?, ?, ?, ?)"));
		pstmt_insert->setInt(1, newId);
		pstmt_insert->setString(2, name);
		pstmt_insert->setString(3, email);
		pstmt_insert->setString(4, pwd);
		// pstmt_insert->setString(5, name);
		// pstmt_insert->setString(6, icon);
		pstmt_insert->executeUpdate();
		con->_con->commit();
		std::cout << "newuser insert into user success" << std::endl;
		return newId;
	}
	catch (sql::SQLException &e)
	{
		if (con)
		{
			con->_con->rollback();
		}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string &name, const std::string &email)
{
	auto con = pool_->getConnection();
	try
	{
		if (con == nullptr)
		{
			return false;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));

		pstmt->setString(1, name);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		while (res->next())
		{
			std::cout << "Check Email: " << res->getString("email") << std::endl;
			if (email != res->getString("email"))
			{
				pool_->returnConnection(std::move(con));
				return false;
			}
			pool_->returnConnection(std::move(con));
			return true;
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string &name, const std::string &newpwd)
{
	auto con = pool_->getConnection();
	try
	{
		if (con == nullptr)
		{
			return false;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		int updateCount = pstmt->executeUpdate();

		std::cout << "Updated rows: " << updateCount << std::endl;
		pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException &e)
	{
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string &email, const std::string &pwd, UserInfo &userInfo)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE email = ?"));
		pstmt->setString(1, email);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		while (res->next())
		{
			origin_pwd = res->getString("pwd");
			std::cout << "Password: " << origin_pwd << std::endl;
			break;
		}

		if (pwd != origin_pwd)
		{
			return false;
		}
		userInfo.name = res->getString("name");
		userInfo.email = res->getString("email");
		userInfo.uid = res->getInt("uid");
		userInfo.pwd = origin_pwd;
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::TestProcedure(const std::string &email, int &uid, std::string &name)
{
	auto con = pool_->getConnection();
	try
	{
		if (con == nullptr)
		{
			return false;
		}

		Defer defer([this, &con]()
					{ pool_->returnConnection(std::move(con)); });
		std::unique_ptr<sql::PreparedStatement> stmt(con->_con->prepareStatement("CALL test_procedure(?,@userId,@userName)"));
		stmt->setString(1, email);

		stmt->execute();
		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @userId AS uid"));
		if (!(res->next()))
		{
			return false;
		}

		uid = res->getInt("uid");
		std::cout << "uid: " << uid << std::endl;

		stmtResult.reset(con->_con->createStatement());
		res.reset(stmtResult->executeQuery("SELECT @userName AS name"));
		if (!(res->next()))
		{
			return false;
		}

		name = res->getString("name");
		std::cout << "name: " << name << std::endl;
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::AddFriendApply(const int &from, const int &to,
							  const std::string &desc, const std::string &back_name)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(
			con->_con->prepareStatement("INSERT INTO friend_apply (from_uid, to_uid, descs, back_name) "
										"values (?,?,?,?) "
										"ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid, descs = ?, back_name = ?"));
		pstmt->setInt(1, from); // from id
		pstmt->setInt(2, to);
		pstmt->setString(3, desc);
		pstmt->setString(4, back_name);
		pstmt->setString(5, desc);
		pstmt->setString(6, back_name);
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0)
		{
			return false;
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}

bool MysqlDao::AuthFriendApply(const int &from, const int &to)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE friend_apply SET status = 1 "
																				  "WHERE from_uid = ? AND to_uid = ?"));
		pstmt->setInt(1, to); // from id
		pstmt->setInt(2, from);
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0)
		{
			return false;
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}

bool MysqlDao::AddFriend(const int &from, const int &to, std::string back_name,
						 std::vector<std::shared_ptr<AddFriendMsg>> &chat_datas)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;

		{
			std::unique_ptr<sql::PreparedStatement> selStmt(con->_con->prepareStatement(
				"SELECT back_name, descs "
				"FROM friend_apply "
				"WHERE from_uid = ? AND to_uid = ? "
				"FOR UPDATE"));
			selStmt->setInt(1, to);
			selStmt->setInt(2, from);

			std::unique_ptr<sql::ResultSet> rsSel(selStmt->executeQuery());

			if (rsSel->next())
			{
				reverse_back = rsSel->getString("back_name");
				apply_desc = rsSel->getString("descs");
			}
			else
			{
				con->_con->rollback();
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"));

			updStmt->setInt(1, to);
			updStmt->setInt(2, from);

			if (updStmt->executeUpdate() != 1)
			{
				con->_con->rollback();
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
																					  "VALUES (?, ?, ?) "));
			pstmt->setInt(1, from); // from id
			pstmt->setInt(2, to);
			pstmt->setString(3, back_name);
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0)
			{
				con->_con->rollback();
				return false;
			}

			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
																					   "VALUES (?, ?, ?) "));
			pstmt2->setInt(1, to); // from id
			pstmt2->setInt(2, from);
			pstmt2->setString(3, reverse_back);
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0)
			{
				con->_con->rollback();
				return false;
			}
		}

		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());"));

			threadStmt->executeUpdate();

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()"));

			if (rs->next())
			{
				threadId = rs->getInt64(1);
			}
			else
			{
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"));

			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);
			if (pcStmt->executeUpdate() < 0)
				return false;
		}

		if (apply_desc.empty() == false)
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content,created_at, updated_at, status, msg_type) VALUES (?, ?, ?, ?,NOW(),NOW(),?, ?)"));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			msgStmt->setInt(5, 2);
			msgStmt->setInt(6, int(ChatMsgType::TEXT));
			if (msgStmt->executeUpdate() < 0)
			{
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()"));
			if (rs->next())
			{
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(to);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent(apply_desc);
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				std::cout << "addfriend insert message success" << std::endl;
				chat_datas.push_back(tx_data);
			}
			else
			{
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status, msg_type) VALUES (?, ?, ?, ?,NOW(),NOW(),?, ?)"));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");

			msgStmt->setInt(5, 2);
			msgStmt->setInt(6, int(ChatMsgType::TEXT));
			if (msgStmt->executeUpdate() < 0)
			{
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()"));
			if (rs->next())
			{
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(from);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent("We are friends now!");
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				chat_datas.push_back(tx_data);
			}
			else
			{
				return false;
			}
		}

		con->_con->commit();
		std::cout << "addfriend insert friends success" << std::endl;

		return true;
	}
	catch (sql::SQLException &e)
	{
		if (con)
		{
			con->_con->rollback();
		}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return nullptr;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		while (res->next())
		{
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->icon = res->getString("icon");
			user_ptr->uid = uid;
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return nullptr;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		while (res->next())
		{
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->uid = res->getInt("uid");
			user_ptr->icon = res->getString("icon");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>> &applyList, int begin, int limit)
{
	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select apply.from_uid, apply.status, user.name, "
																				  "user.nick, user.sex, user.icon, user.desc from friend_apply as apply join user on apply.from_uid = user.uid where apply.to_uid = ? "
																				  "and apply.id > ? order by apply.id ASC LIMIT ? "));

		pstmt->setInt(1, touid);
		pstmt->setInt(2, begin);
		pstmt->setInt(3, limit);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next())
		{
			auto name = res->getString("name");
			auto uid = res->getInt("from_uid");
			auto icon = res->getString("icon");
			auto desc = res->getString("desc");
			auto status = res->getInt("status");
			auto nick = res->getString("nick");
			auto sex = res->getInt("sex");
			auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, desc, icon, nick, sex, status);
			applyList.push_back(apply_ptr);
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info_list)
{

	auto con = pool_->getConnection();
	if (con == nullptr)
	{
		return false;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ? "));

		pstmt->setInt(1, self_id);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next())
		{
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr)
			{
				continue;
			}

			user_info->back = user_info->name;
			user_info_list.push_back(user_info);
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}

bool MysqlDao::GetUserThreads(
	int64_t userId,
	int64_t lastId,
	int pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>> &threads,
	bool &loadMore,
	int &nextLastId)
{
	loadMore = false;
	nextLastId = lastId;
	threads.clear();

	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });
	auto &conn = con->_con;

	try
	{
		std::string sql =
			"SELECT thread_id, 'private' AS type, user1_id, user2_id "
			"  FROM private_chat "
			" WHERE (user1_id = ? OR user2_id = ?) "
			"   AND thread_id > ? "
			"UNION ALL "
			"SELECT thread_id, 'group' AS type, 0 AS user1_id, 0 AS user2_id "
			"  FROM group_chat_member "
			" WHERE user_id = ? "
			"   AND thread_id > ? "
			"ORDER BY thread_id "
			"LIMIT ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(
			conn->prepareStatement(sql));

		int idx = 1;
		pstmt->setInt64(idx++, userId);
		pstmt->setInt64(idx++, userId);
		pstmt->setInt64(idx++, lastId);
		pstmt->setInt64(idx++, userId);
		pstmt->setInt64(idx++, lastId);
		pstmt->setInt(idx++, pageSize + 1);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		std::vector<std::shared_ptr<ChatThreadInfo>> tmp;
		while (res->next())
		{
			auto cti = std::make_shared<ChatThreadInfo>();
			cti->_thread_id = res->getInt64("thread_id");
			cti->_type = res->getString("type");
			cti->_user1_id = res->getInt64("user1_id");
			cti->_user2_id = res->getInt64("user2_id");
			tmp.push_back(cti);
		}

		if ((int)tmp.size() > pageSize)
		{
			loadMore = true;
			tmp.pop_back();
		}

		if (!tmp.empty())
		{
			nextLastId = tmp.back()->_thread_id;
		}

		threads = std::move(tmp);
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what()
				  << " (MySQL error code: " << e.getErrorCode()
				  << ", SQLState: " << e.getSQLState() << ")\n";
		return false;
	}

	return true;
}

bool MysqlDao::CreatePrivateChat(int user1_id, int user2_id, int &thread_id)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });
	auto &conn = con->_con;
	try
	{
		conn->setAutoCommit(false);
		int uid1 = std::min(user1_id, user2_id);
		int uid2 = std::max(user1_id, user2_id);
		std::string check_sql =
			"SELECT thread_id FROM private_chat "
			"WHERE (user1_id = ? AND user2_id = ?) "
			"FOR UPDATE;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(check_sql));
		pstmt->setInt64(1, uid1);
		pstmt->setInt64(2, uid2);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		if (res->next())
		{
			thread_id = res->getInt("thread_id");
			conn->commit();
			return true;
		}

		std::string insert_chat_thread_sql =
			"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_thread(conn->prepareStatement(insert_chat_thread_sql));
		pstmt_insert_thread->executeUpdate();

		std::string get_last_insert_id_sql = "SELECT LAST_INSERT_ID();";
		std::unique_ptr<sql::PreparedStatement> pstmt_last_insert_id(conn->prepareStatement(get_last_insert_id_sql));
		std::unique_ptr<sql::ResultSet> res_last_id(pstmt_last_insert_id->executeQuery());
		res_last_id->next();
		thread_id = res_last_id->getInt(1);

		std::string insert_private_chat_sql =
			"INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
			"VALUES (?, ?, ?, NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_private(conn->prepareStatement(insert_private_chat_sql));
		pstmt_insert_private->setInt64(1, thread_id);
		pstmt_insert_private->setInt64(2, uid1);
		pstmt_insert_private->setInt64(3, uid2);
		pstmt_insert_private->executeUpdate();

		conn->commit();
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return false;
	}
	return false;
}

std::shared_ptr<PageResult> MysqlDao::LoadChatMsg(int thread_id, int last_message_id, int page_size)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return nullptr;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });
	auto &conn = con->_con;

	try
	{
		auto page_res = std::make_shared<PageResult>();
		page_res->load_more = false;
		const std::string sql = R"(
        SELECT message_id, thread_id, sender_id, recv_id, content,
               created_at, updated_at, status,msg_type
        FROM chat_message
        WHERE thread_id = ?
          AND message_id > ?
        ORDER BY message_id ASC
        LIMIT ?
		)";

		uint32_t fetch_limit = page_size + 1;
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(sql));
		pstmt->setInt(1, thread_id);
		pstmt->setInt(2, last_message_id);
		pstmt->setInt(3, fetch_limit);

		auto rs = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());

		while (rs->next())
		{
			ChatMessage msg;
			msg.message_id = rs->getUInt64("message_id");
			msg.thread_id = rs->getUInt64("thread_id");
			msg.sender_id = rs->getUInt64("sender_id");
			msg.recv_id = rs->getUInt64("recv_id");
			msg.content = rs->getString("content");
			msg.chat_time = rs->getString("created_at");
			msg.status = rs->getInt("status");
			msg.msg_type = rs->getInt("msg_type");
			page_res->messages.push_back(std::move(msg));
		}
		if (page_res->messages.size() > page_size)
		{
			page_res->messages.pop_back();
			page_res->load_more = true;
		}

		if (!page_res->messages.empty())
		{
			page_res->next_cursor = page_res->messages.back().message_id;
		}
		else
		{
			page_res->next_cursor = last_message_id;
		}

		return page_res;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return nullptr;
	}
	return nullptr;
}

bool MysqlDao::AddChatMsg(std::vector<std::shared_ptr<ChatMessage>> &chat_datas)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });
	auto &conn = con->_con;

	try
	{
		conn->setAutoCommit(false);
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"INSERT INTO chat_message "
				"(thread_id, sender_id, recv_id, content, created_at, updated_at, status,msg_type) "
				"VALUES (?, ?, ?, ?, ?, ?, ?,?)"));

		for (auto &msg : chat_datas)
		{
			pstmt->setUInt64(1, msg->thread_id);
			pstmt->setUInt64(2, msg->sender_id);
			pstmt->setUInt64(3, msg->recv_id);
			pstmt->setString(4, msg->content);

			pstmt->setString(5, msg->chat_time); // created_at
			pstmt->setString(6, msg->chat_time); // updated_at

			pstmt->setInt(7, msg->status);
			pstmt->setInt(8, msg->msg_type);
			pstmt->executeUpdate();

			std::unique_ptr<sql::Statement> keyStmt(
				conn->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				keyStmt->executeQuery("SELECT LAST_INSERT_ID()"));
			if (rs->next())
			{
				msg->message_id = rs->getUInt64(1);
			}
			else
			{
				continue;
			}
		}

		conn->commit();
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return false;
	}
	return true;
}

bool MysqlDao::AddChatMsg(std::shared_ptr<ChatMessage> chat_data)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });
	auto &conn = con->_con;

	try
	{
		conn->setAutoCommit(false);
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"INSERT INTO chat_message "
				"(thread_id, sender_id, recv_id, content, created_at, updated_at, status,msg_type) "
				"VALUES (?, ?, ?, ?, ?, ?, ?,?)"));

		pstmt->setUInt64(1, chat_data->thread_id);
		pstmt->setUInt64(2, chat_data->sender_id);
		pstmt->setUInt64(3, chat_data->recv_id);
		pstmt->setString(4, chat_data->content);
		pstmt->setString(5, chat_data->chat_time); // created_at
		pstmt->setString(6, chat_data->chat_time); // updated_at
		pstmt->setInt(7, chat_data->status);
		pstmt->setInt(8, chat_data->msg_type);

		pstmt->executeUpdate();

		std::unique_ptr<sql::Statement> keyStmt(
			conn->createStatement());
		std::unique_ptr<sql::ResultSet> rs(
			keyStmt->executeQuery("SELECT LAST_INSERT_ID()"));
		if (rs->next())
		{
			chat_data->message_id = rs->getUInt64(1);
		}

		conn->commit();
		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return false;
	}
}

std::shared_ptr<ChatMessage> MysqlDao::GetChatMsg(int message_id)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return nullptr;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	auto &conn = con->_con;

	try
	{
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"SELECT message_id, thread_id, sender_id, recv_id, "
				"content, created_at, updated_at, status , msg_type"
				"FROM chat_message WHERE message_id = ?"));

		pstmt->setUInt64(1, message_id);
		auto rs = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());

		if (rs->next())
		{
			auto msg = std::make_shared<ChatMessage>();
			msg->message_id = rs->getUInt64("message_id");
			msg->thread_id = rs->getUInt64("thread_id");
			msg->sender_id = rs->getUInt64("sender_id");
			msg->recv_id = rs->getUInt64("recv_id");
			msg->content = rs->getString("content");
			msg->chat_time = rs->getString("created_at");
			msg->status = rs->getInt("status");
			msg->msg_type = rs->getInt("msg_type");

			return msg;
		}

		return nullptr;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "GetChatMessageById SQLException: " << e.what() << std::endl;
		return nullptr;
	}
}

bool MysqlDao::UpdateHeadInfo(int uid, const std::string &icon)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	auto &conn = con->_con;
	try
	{
		std::string update_sql =
			"UPDATE user SET icon = ? WHERE uid = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(update_sql));
		pstmt->setString(1, icon);
		pstmt->setInt64(2, uid);

		int affected_rows = pstmt->executeUpdate();

		if (affected_rows == 0)
		{
			std::cerr << "No user found with uid: " << uid << std::endl;
			return false;
		}

		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException in UpdateHeadInfo: " << e.what() << std::endl;
		return false;
	}
	return false;
}

bool MysqlDao::UpdateUploadStatus(int chat_message_id)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return false;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	auto &conn = con->_con;
	try
	{
		std::string update_sql =
			"UPDATE chat_message SET status = ? WHERE message_id = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(update_sql));
		pstmt->setInt(1, MsgStatus::READED);
		pstmt->setInt64(2, chat_message_id);

		int affected_rows = pstmt->executeUpdate();

		if (affected_rows == 0)
		{
			std::cerr << "No chat message found with chat_message_id: " << chat_message_id << std::endl;
			return false;
		}

		return true;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException in UpdateUploadStatus: " << e.what() << std::endl;
		return false;
	}
	return false;
}

std::shared_ptr<ChatImgInfo> MysqlDao::GetImgInfoByMsgId(int message_id)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return nullptr;
	}
	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	auto &conn = con->_con;
	try
	{
		std::string update_sql =
			"select message_id, sender_id, recv_id, content from  WHERE message_id = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(update_sql));

		pstmt->setInt(1, message_id);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next())
		{
			auto message_id = res->getInt64("message_id");
			auto sender_id = res->getInt64("sender_id");
			auto recv_id = res->getInt64("recv_id");
			auto img_name = res->getString("content");
			auto img_info = std::make_shared<ChatImgInfo>(sender_id, recv_id, message_id, img_name);
			return img_info;
		}
		return nullptr;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "SQLException in UpdateUploadStatus: " << e.what() << std::endl;
		return nullptr;
	}
	return nullptr;
}

std::shared_ptr<ChatMessage> MysqlDao::GetChatMsgById(int message_id)
{
	auto con = pool_->getConnection();
	if (!con)
	{
		return nullptr;
	}

	Defer defer([this, &con]()
				{ pool_->returnConnection(std::move(con)); });

	auto &conn = con->_con;

	try
	{
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"SELECT message_id, thread_id, sender_id, recv_id, "
				"content, created_at, updated_at, status , msg_type "
				"FROM chat_message WHERE message_id = ?"));

		pstmt->setUInt64(1, message_id);
		auto rs = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());

		if (rs->next())
		{
			auto msg = std::make_shared<ChatMessage>();
			msg->message_id = rs->getUInt64("message_id");
			msg->thread_id = rs->getUInt64("thread_id");
			msg->sender_id = rs->getUInt64("sender_id");
			msg->recv_id = rs->getUInt64("recv_id");
			msg->content = rs->getString("content");
			msg->chat_time = rs->getString("created_at");
			msg->status = rs->getInt("status");
			msg->msg_type = rs->getInt("msg_type");
			return msg;
		}

		return nullptr;
	}
	catch (sql::SQLException &e)
	{
		std::cerr << "GetChatMessageById SQLException: " << e.what() << std::endl;
		return nullptr;
	}
}
