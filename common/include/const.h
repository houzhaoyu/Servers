#pragma once

#include <memory>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include <cassert>
#include <queue>
#include <atomic>


#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "hiredis.h"

#include "jsoncpp/json/json.h"
#include "jsoncpp/json/json-forwards.h"

#include "ConfigMgr.h"
#include "RedisMgr.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC请求错误
	VerifyExpired = 1003, //验证码过期
	VerifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已经存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码更新失败
	TokenInvalid = 1010,   //Token失效
	UidInvalid = 1011,  //uid无效
	CREATE_CHAT_FAILED = 1012, //创建聊天失败
	LOAD_CHAT_FAILED = 1013, //加载聊天失败

	CreateFilePathFailed = 1014, //文件路径创建失败
	FileWritePermissionFailed = 1015,  //文件写权限不足
	FileReadPermissionFailed = 1016,  //文件读权限不足
	FileSeqInvalid = 1017,     //文件序列有误
	FileOffsetInvalid = 1018,   //文件偏移量有误
	FileReadFailed = 1019,      //文件读取失败
	RedisReadErr = 1020,        //redis读取失败
	ServerIpErr = 1021,          //server ip错误
	MsgIdErr = 1022,             //消息id错误

	FileNotExists = 1023, //文件不存在
	FileSaveRedisFailed = 1024, //文件存储redis失败
};

//头部id长度
#define HEAD_ID_LEN 2
//聊天服务器消息头
//头部总长度
#define CHAT_HEAD_TOTAL_LEN 4
//头部数据长度
#define CHAT_HEAD_DATA_LEN 2

#define CHAT_MAX_LENGTH  1024*2

#define CHAT_MAX_RECVQUE  10000
#define CHAT_MAX_SENDQUE 1000

//资源服务器消息头
//头部总长度
#define FILE_HEAD_TOTAL_LEN 6
//头部数据长度
#define FILE_HEAD_DATA_LEN 4

#define FILE_MAX_LENGTH 1024 * 50

#define FILE_MAX_RECVQUE 2000000
#define FILE_MAX_SENDQUE 2000000

enum MSG_IDS {
    ID_GET_VERIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002, //注册用户
    ID_RESET_PWD = 1003, //重置密码
    ID_LOGIN_USER = 1004, //用户登录
    ID_CHAT_LOGIN = 1005, //登陆聊天服务器
    ID_CHAT_LOGIN_RSP = 1006, //登陆聊天服务器回包
    ID_SEARCH_USER_REQ = 1007, //用户搜索请求
    ID_SEARCH_USER_RSP = 1008, //搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,  //添加好友申请
    ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请
    ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
    ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请
    ID_TEXT_CHAT_MSG_REQ = 1017,  //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP = 1018,  //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
    ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
    ID_HEART_BEAT_REQ = 1023,      //心跳请求
    ID_HEARTBEAT_RSP = 1024,       //心跳回复
    ID_LOAD_CHAT_THREAD_REQ = 1025,      //加载聊天线程
    ID_LOAD_CHAT_THREAD_RSP = 1026,      //加载聊天线程回复
    ID_CREATE_PRIVATE_CHAT_REQ = 1027, //创建私聊请求
    ID_CREATE_PRIVATE_CHAT_RSP = 1028, //创建私聊回复
    ID_LOAD_CHAT_MSG_REQ = 1029,      //加载聊天消息
    ID_LOAD_CHAT_MSG_RSP = 1030,      //加载聊天消息
    ID_UPLOAD_HEAD_ICON_REQ = 1031,      //上传头像请求
    ID_UPLOAD_HEAD_ICON_RSP = 1032,      //上传头像回复
    ID_DOWN_LOAD_FILE_REQ = 1033,             //下载文件请求
    ID_DOWN_LOAD_FILE_RSP = 1034,           //下载文件回复
    ID_IMG_CHAT_MSG_REQ = 1035,            //图片聊天消息请求
    ID_IMG_CHAT_MSG_RSP = 1036,           //图片聊天信息回复
    ID_IMG_CHAT_UPLOAD_REQ = 1037,        //上传聊天图片资源
    ID_IMG_CHAT_UPLOAD_RSP = 1038,        //上传聊天图片资源回复

    ID_NOTIFY_IMG_CHAT_MSG_REQ = 1039,   //通知用户图片聊天信息
    ID_FILE_INFO_SYNC_REQ = 1041,    //文件信息同步请求
    ID_FILE_INFO_SYNC_RSP = 1042,     //文件信息同步回复
    ID_IMG_CHAT_CONTINUE_UPLOAD_REQ = 1043,  //续传聊天图片资源请求
    ID_IMG_CHAT_CONTINUE_UPLOAD_RSP = 1044,  //续传聊天图片资源回复
    ID_IMG_CHAT_DOWN_INFO_SYNC_REQ = 1045,  //获取图片下载信息同步请求
    ID_IMG_CHAT_DOWN_INFO_SYNC_RSP = 1046,  //获取图片下载信息同步回复
    ID_IMG_CHAT_DOWN_REQ = 1047,    //聊天图片下载请求
    ID_IMG_CHAT_DOWN_RSP = 1048     //聊天图片下载回复
};

#define USER_IP_PREFIX  "uip_"
#define USER_TOKEN_PREFIX  "utoken_"
#define IP_COUNT_PREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define NAME_INFO  "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"
#define CODE_PREFIX  "code_"

//分布式锁的持有时间
#define LOCK_TIME_OUT 10
//分布式锁的重试时间
#define ACQUIRE_TIME_OUT 5

//心跳间隔
#define HEARTBEAT_INTERVAL 60
//心跳阈值
#define HEART_THRESHOLD 20

//资源服务器
//4个逻辑工作者
#define LOGIC_WORKER_COUNT 4
//4个文件工作者
#define FILE_WORKER_COUNT 4
//4个下载工作者
#define DOWN_LOAD_WORKER_COUNT	4
//最大传输文件的大小
#define MAX_FILE_LEN 2048

enum MsgStatus {
	UN_READ = 0,  //对方未读
	SEND_FAILED = 1,  //发送失败
	READED = 2,  //对方已读
	UN_UPLOAD = 3 //未上传完成
};