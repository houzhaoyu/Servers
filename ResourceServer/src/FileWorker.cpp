#include "FileWorker.h"
#include "FileSession.h"
#include <json/json.h>
#include "jsoncpp/json/json.h"
#include "jsoncpp/json/json-forwards.h"
#include "base64.h"
#include "ConfigMgr.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "ChatServerGrpcClient.h"

FileWorker::FileWorker() : _b_stop(false)
{
	RegisterHandlers();
	_work_thread = std::thread([this]()
		{
			std::thread::id thread_id = std::this_thread::get_id();
			std::stringstream ss;
			ss << thread_id;
			std::string trace_id = ss.str();

			LogContext::SetTraceId(trace_id);
			Defer defer([]()
				{ LogContext::Clear(); });

			while (!_b_stop)
			{
				std::unique_lock<std::mutex> lock(_mtx);
				_cv.wait(lock, [this]()
					{
						if (_b_stop) {
							return true;
						}

						if (_task_que.empty()) {
							return false;
						}

						return true; });

				if (_b_stop)
				{
					break;
				}

				auto task_call = _task_que.front();
				_task_que.pop();
				task_call();
			} });
}

FileWorker::~FileWorker()
{
	_b_stop = true;
	_cv.notify_one();
	_work_thread.join();
}

void FileWorker::RegisterHandlers()
{
	// 处理头像上传
	_handlers[ID_UPLOAD_HEAD_ICON_REQ] = [this](std::shared_ptr<FileTask> task)
		{
			Logger::Debug("FileWorker::RegisterHandlers - receive upload head icon req, uid is {}, file name is {}", task->_uid, task->_name);
			// 解码
			std::string decoded = base64_decode(task->_file_data);

			auto file_path_str = task->_path;
			auto last = task->_last;

			boost::filesystem::path file_path(file_path_str);
			boost::filesystem::path dir_path = file_path.parent_path();
			// 获取完整文件名（包含扩展名）
			std::string filename = file_path.filename().string();
			Json::Value result;
			result["error"] = ErrorCodes::Success;

			// Check if directory exists, if not, create it
			if (!boost::filesystem::exists(dir_path))
			{
				if (!boost::filesystem::create_directories(dir_path))
				{
					std::cerr << "Failed to create directory: " << dir_path.string() << std::endl;
					result["error"] = ErrorCodes::FileNotExists;
					task->_callback(result);
					return;
				}
			}

			std::ofstream outfile;
			// 第一个包
			if (task->_seq == 1)
			{
				// 打开文件，如果存在则清空，不存在则创建
				outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
			}
			else
			{
				// 保存为文件
				outfile.open(file_path_str, std::ios::binary | std::ios::app);
			}

			if (!outfile)
			{
				Logger::Error("无法打开文件进行写入: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.write(decoded.data(), decoded.size());
			if (!outfile)
			{
				Logger::Error("写入文件失败: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.close();
			if (last)
			{
				Logger::Debug("文件已成功保存为: {}", task->_name);
				// 更新头像
				MysqlMgr::GetInstance()->UpdateUserIcon(task->_uid, filename);
				// 获取用户信息
				auto user_info = MysqlMgr::GetInstance()->GetUser(task->_uid);
				if (user_info == nullptr)
				{
					return;
				}

				// 将数据库内容写入redis缓存
				Json::Value redis_root;
				redis_root["uid"] = task->_uid;
				redis_root["pwd"] = user_info->pwd;
				redis_root["name"] = user_info->name;
				redis_root["email"] = user_info->email;
				redis_root["nick"] = user_info->nick;
				redis_root["desc"] = user_info->desc;
				redis_root["sex"] = user_info->sex;
				redis_root["icon"] = user_info->icon;
				std::string base_key = USER_BASE_INFO + std::to_string(task->_uid);
				RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
			}

			if (task->_callback)
			{
				task->_callback(result);
			}
		};

	// 处理聊天图片上传
	_handlers[ID_IMG_CHAT_UPLOAD_REQ] = [this](std::shared_ptr<FileTask> task)
		{
			Logger::Debug("FileWorker::RegisterHandlers - receive upload chat image req, uid is {}, file name is {}", task->_uid, task->_name);
			// 解码
			std::string decoded = base64_decode(task->_file_data);

			auto file_path_str = task->_path;
			auto last = task->_last;
			// std::cout << "file_path_str is " << file_path_str << std::endl;

			boost::filesystem::path file_path(file_path_str);
			boost::filesystem::path dir_path = file_path.parent_path();
			// 获取完整文件名（包含扩展名）
			std::string filename = file_path.filename().string();
			Json::Value result;
			result["error"] = ErrorCodes::Success;

			// Check if directory exists, if not, create it
			if (!boost::filesystem::exists(dir_path))
			{
				if (!boost::filesystem::create_directories(dir_path))
				{
					std::cerr << "Failed to create directory: " << dir_path.string() << std::endl;
					result["error"] = ErrorCodes::FileNotExists;
					task->_callback(result);
					return;
				}
			}

			std::ofstream outfile;
			// 第一个包
			if (task->_seq == 1)
			{
				// 打开文件，如果存在则清空，不存在则创建
				outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
			}
			else
			{
				// 保存为文件
				outfile.open(file_path_str, std::ios::binary | std::ios::app);
			}

			if (!outfile)
			{
				Logger::Error("无法打开文件进行写入: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.write(decoded.data(), decoded.size());
			if (!outfile)
			{
				Logger::Error("写入文件失败: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.close();
			if (last)
			{
				Logger::Debug("文件已成功保存为: {}", task->_name);
				// 更新数据库聊天图像上传状态
				MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);

				std::string uid_ip_value = "";
				auto receiver_str = std::to_string(task->_receiver);
				auto uid_ip_key = USER_IP_PREFIX + receiver_str;
				bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
				// 如果接收者未登录，则直接返回
				if (!b_ip)
				{
					if (task->_callback)
					{
						task->_callback(result);
					}

					return;
				}

				if (task->_callback)
				{
					task->_callback(result);
				}

				// 通过grpc通知ChatServer
				ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
				return;
			}

			if (task->_callback)
			{
				task->_callback(result);
			}
		};

	// 处理文件信息同步请求
	_handlers[ID_FILE_INFO_SYNC_REQ] = [this](std::shared_ptr<FileTask> task)
		{
			// 解码
			std::string decoded = base64_decode(task->_file_data);

			auto file_path_str = task->_path;
			auto last = task->_last;
			// std::cout << "file_path_str is " << file_path_str << std::endl;

			boost::filesystem::path file_path(file_path_str);
			boost::filesystem::path dir_path = file_path.parent_path();
			// 获取完整文件名（包含扩展名）
			std::string filename = file_path.filename().string();
			Json::Value result;
			result["error"] = ErrorCodes::Success;

			// Check if directory exists, if not, create it
			if (!boost::filesystem::exists(dir_path))
			{
				if (!boost::filesystem::create_directories(dir_path))
				{
					std::cerr << "Failed to create directory: " << dir_path.string() << std::endl;
					result["error"] = ErrorCodes::FileNotExists;
					task->_callback(result);
					return;
				}
			}

			std::ofstream outfile;
			// 第一个包
			if (task->_seq == 1)
			{
				// 打开文件，如果存在则清空，不存在则创建
				outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
			}
			else
			{
				// 保存为文件
				outfile.open(file_path_str, std::ios::binary | std::ios::app);
			}

			if (!outfile)
			{
				Logger::Error("无法打开文件进行写入: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.write(decoded.data(), decoded.size());
			if (!outfile)
			{
				Logger::Error("写入文件失败: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.close();
			if (last)
			{
				Logger::Debug("文件已成功保存为: {}", task->_name);
				// todo...更新数据库聊天图像上传状态
				MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);
				std::string uid_ip_value = "";
				auto receiver_str = std::to_string(task->_receiver);
				auto uid_ip_key = USER_IP_PREFIX + receiver_str;
				bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
				// 如果接收者未登录，则直接返回
				if (!b_ip)
				{
					if (task->_callback)
					{
						task->_callback(result);
					}

					return;
				}

				if (task->_callback)
				{
					task->_callback(result);
				}

				// 通过grpc通知ChatServer
				ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
				return;
			}

			if (task->_callback)
			{
				task->_callback(result);
			}
		};

	// 处理续传图片请求
	_handlers[ID_IMG_CHAT_CONTINUE_UPLOAD_REQ] = [this](std::shared_ptr<FileTask> task)
		{
			Logger::Debug("FileWorker::RegisterHandlers - receive continue upload chat image req, uid is {}, file name is {}", task->_uid, task->_name);
			// 解码
			std::string decoded = base64_decode(task->_file_data);

			auto file_path_str = task->_path;
			auto last = task->_last;
			// std::cout << "file_path_str is " << file_path_str << std::endl;

			boost::filesystem::path file_path(file_path_str);
			boost::filesystem::path dir_path = file_path.parent_path();
			// 获取完整文件名（包含扩展名）
			std::string filename = file_path.filename().string();
			Json::Value result;
			result["error"] = ErrorCodes::Success;

			// Check if directory exists, if not, create it
			if (!boost::filesystem::exists(dir_path))
			{
				if (!boost::filesystem::create_directories(dir_path))
				{
					Logger::Error("Failed to create directory: {}", dir_path.string());
					result["error"] = ErrorCodes::FileNotExists;
					task->_callback(result);
					return;
				}
			}

			std::ofstream outfile;
			// 第一个包
			if (task->_seq == 1)
			{
				// 打开文件，如果存在则清空，不存在则创建
				outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
			}
			else
			{
				// 保存为文件
				outfile.open(file_path_str, std::ios::binary | std::ios::app);
			}

			if (!outfile)
			{
				Logger::Error("无法打开文件进行写入: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.write(decoded.data(), decoded.size());
			if (!outfile)
			{
				Logger::Error("写入文件失败: {}", file_path_str);
				result["error"] = ErrorCodes::FileWritePermissionFailed;
				task->_callback(result);
				return;
			}

			outfile.close();
			if (last)
			{
				Logger::Debug("文件已成功保存为: {}", task->_name);
				// 更新数据库聊天图像上传状态
				MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);

				std::string uid_ip_value = "";
				auto receiver_str = std::to_string(task->_receiver);
				auto uid_ip_key = USER_IP_PREFIX + receiver_str;
				bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
				// 如果接收者未登录，则直接返回
				if (!b_ip)
				{
					if (task->_callback)
					{
						task->_callback(result);
					}

					return;
				}

				// 通过grpc通知ChatServer
				ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
				if (task->_callback)
				{
					task->_callback(result);
				}

				return;
			}

			if (task->_callback)
			{
				task->_callback(result);
			}
		};
}

void FileWorker::PostTask(std::shared_ptr<FileTask> task)
{
	{
		std::lock_guard<std::mutex> lock(_mtx);
		// 借鉴python万物皆对象思想，构造伪闭包将函数对象扔到队列中
		_task_que.push([task, this]()
			{ task_callback(task); });
	}

	_cv.notify_one();
}

void FileWorker::task_callback(std::shared_ptr<FileTask> task)
{
	auto iter = _handlers.find(task->_msg_id);
	if (iter == _handlers.end())
	{
		return;
	}

	iter->second(task);
}

DownloadWorker::DownloadWorker() : _b_stop(false)
{
	_work_thread = std::thread([this]()
		{
			while (!_b_stop) {
				std::unique_lock<std::mutex> lock(_mtx);
				_cv.wait(lock, [this]() {
					if (_b_stop) {
						return true;
					}

					if (_task_que.empty()) {
						return false;
					}

					return true;
					});

				if (_b_stop) {
					break;
				}

				auto task = _task_que.front();
				_task_que.pop();
				task_callback(task);
			} });
}

DownloadWorker::~DownloadWorker()
{
	_b_stop = true;
	_cv.notify_one();
	_work_thread.join();
}

void DownloadWorker::PostTask(std::shared_ptr<DownloadTask> task)
{
	{
		std::lock_guard<std::mutex> lock(_mtx);
		_task_que.push(task);
	}

	_cv.notify_one();
}

void DownloadWorker::task_callback(std::shared_ptr<DownloadTask> task)
{
	// 解码
	auto file_path_str = task->_file_path;

	boost::filesystem::path file_path(file_path_str);

	Json::Value result;
	result["error"] = ErrorCodes::Success;

	if (!boost::filesystem::exists(file_path))
	{
		Logger::Error("文件不存在: {}", file_path_str);
		result["error"] = ErrorCodes::FileNotExists;
		task->_callback(result);
		return;
	}

	std::ifstream infile(file_path_str, std::ios::binary);
	if (!infile)
	{
		Logger::Error("无法打开文件进行读取: {}", file_path_str);
		result["error"] = ErrorCodes::FileReadPermissionFailed;
		task->_callback(result);
		return;
	}

	std::shared_ptr<FileInfo> file_info = nullptr;

	if (task->_seq == 1)
	{
		// 获取文件大小
		infile.seekg(0, std::ios::end);
		std::streamsize file_size = infile.tellg();
		infile.seekg(0, std::ios::beg);
		// 如果为空，则创建FileInfo 构造数据存储
		file_info = std::make_shared<FileInfo>();
		file_info->_file_path_str = file_path_str;
		file_info->_name = task->_name;
		file_info->_seq = 1;

		file_info->_total_size = file_size;
		file_info->_trans_size = 0;
		// 立即保存到 Redis，覆盖旧数据，设置过期时间
		RedisMgr::GetInstance()->SetDownLoadInfo(task->_name, file_info);
		Logger::Debug("[新下载] 文件: {}, 大小: {} 字节", task->_name, file_size);
	}
	else
	{
		// 断点续传，从 Redis 获取历史信息
		file_info = RedisMgr::GetInstance()->GetDownloadInfo(task->_name);
		if (file_info == nullptr)
		{
			// Redis 中没有信息（可能过期了）
			Logger::Error("断点续传失败，Redis 中无下载信息: {}", task->_name);
			result["error"] = ErrorCodes::RedisReadErr;
			task->_callback(result);
			infile.close();
			return;
		}
		// 验证序列号是否匹配
		if (task->_seq != file_info->_seq)
		{
			Logger::Error("序列号不匹配，期望: {}, 实际: {}", file_info->_seq, task->_seq);
			result["error"] = ErrorCodes::FileSeqInvalid;
			task->_callback(result);
			infile.close();
			return;
		}

		Logger::Debug("[续传] 文件: {}, seq: {}, 进度: {}/{}", task->_name, task->_seq, file_info->_trans_size, file_info->_total_size);
	}

	// 计算当前偏移量
	std::streamsize offset = ((std::streamsize)task->_seq - 1) * MAX_FILE_LEN;
	if (offset >= file_info->_total_size)
	{
		Logger::Error("偏移量超出文件大小: {}", file_path_str);
		result["error"] = ErrorCodes::FileOffsetInvalid;
		task->_callback(result);
		infile.close();
		return;
	}

	// 定位到指定偏移量
	infile.seekg(offset);

	// 读取最多MAX_FILE_LEN字节
	char buffer[MAX_FILE_LEN];
	infile.read(buffer, MAX_FILE_LEN);
	// 获取read实际读取多少字节
	std::streamsize bytes_read = infile.gcount();

	if (bytes_read <= 0)
	{
		Logger::Error("读取文件失败，可能已到达文件末尾: {}", file_path_str);
		result["error"] = ErrorCodes::FileReadFailed;
		task->_callback(result);
		infile.close();
		return;
	}

	// 将读取的数据进行base64编码
	std::string data_to_encode(buffer, bytes_read);
	std::string encoded_data = base64_encode(data_to_encode);

	// 检查是否是最后一个包
	std::streamsize current_pos = offset + bytes_read;
	bool is_last = (current_pos >= file_info->_total_size);

	// 设置返回结果
	result["data"] = encoded_data;
	result["seq"] = task->_seq;
	result["total_size"] = std::to_string(file_info->_total_size);
	result["current_size"] = std::to_string(current_pos);
	result["is_last"] = is_last;

	infile.close();

	if (is_last)
	{
		Logger::Debug("文件读取完成: {}, 大小: {} 字节", task->_name, file_info->_total_size);
		RedisMgr::GetInstance()->DelDownLoadInfo(task->_name);
	}
	else
	{
		// 更新信息
		file_info->_seq++;
		file_info->_trans_size = offset + bytes_read;
		// 更新redis
		RedisMgr::GetInstance()->SetDownLoadInfo(task->_name, file_info);
	}

	if (task->_callback)
	{
		task->_callback(result);
	}
}
