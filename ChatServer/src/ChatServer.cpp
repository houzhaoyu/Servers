#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOContextPool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "Defer.h"
#include "ChatServiceImpl.h"
#include "Logger.h"

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

// ✅ 从命令行参数中解析 "-S servername"
std::string ParseServerName(int argc, char* argv[]) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-S") {
            return std::string(argv[i + 1]);
        }
    }
    return ""; // 未指定
}

int main(int argc, char* argv[])
{
    auto& cfg = ConfigMgr::Inst();

    std::string serverName = ParseServerName(argc, argv);
    // 将 SelfServer 段更新为当前实例配置，作为统一数据源
    cfg.SetSelfServer(serverName);

    auto self = cfg.GetSelfServer();
    serverName = self.GetValue("Name");
    std::string host = self.GetValue("Host");
    std::string port_str = self.GetValue("Port");
    std::string rpc_port = self.GetValue("RPCPort");
    std::string log_level = self.GetValue("LogLevel");

    Logger::Init(serverName);
    Logger::SetLevel(log_level);
    Logger::Info("{} is starting...", serverName);

    try {
        auto pool = AsioIOContextPool::GetInstance();
        //将登录数设置为0
        RedisMgr::GetInstance()->InitCount(serverName);
        // 服务注册：上报自身 host/port/rpcport，供 StatusServer 动态发现
        RedisMgr::GetInstance()->RegisterServer(serverName, host, port_str, rpc_port);
        Defer derfer([serverName]() {
            RedisMgr::GetInstance()->UnregisterServer(serverName);
            RedisMgr::GetInstance()->HDel(LOGIN_COUNT, serverName);
            RedisMgr::GetInstance()->Close();
            });

        boost::asio::io_context  io_context;
        //创建Cserver智能指针
        auto handler = std::bind(&LogicSystem::PostTask, LogicSystem::GetInstance().get(), std::placeholders::_1, std::placeholders::_2);
        auto pointer_server = std::make_shared<CServer>(io_context, atoi(port_str.c_str()), handler);
        //定义一个GrpcServer
        // 监听地址固定用 0.0.0.0：公网 IP 是云厂商 NAT 映射、不在本机网卡上，bind 会失败。
        // config 中的 Host 仅用于对外通告（客户端连接地址），不用于监听。
        std::string server_address("0.0.0.0:" + rpc_port);
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        // 监听端口和添加服务
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        service.RegisterServer(pointer_server);
        // 构建并启动gRPC服务器
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        Logger::Info("RPC Server listening on {}", server_address);

        //单独启动一个线程处理grpc服务
        std::thread  grpc_server_thread([&server]() {
            server->Wait();
            });

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, pool, &server](auto, auto) {
            io_context.stop();
            pool->Stop();
            server->Shutdown();
            });

        //将Cserver注册给逻辑类方便以后清除连接
        LogicSystem::GetInstance()->SetServer(pointer_server);
        io_context.run();

        grpc_server_thread.join();
    }
    catch (std::exception& e) {
        //std::cerr << "Exception: " << e.what() << std::endl;
        Logger::Error("Exception : {}", e.what());
    }
}
