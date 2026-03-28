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
    std::string port_str;
    if (serverName.empty() || !cfg.HasSection(serverName))
    {
        port_str = cfg["SelfServer"]["Port"];
        serverName = cfg["SelfServer"]["Name"];
    }
    else
    {
        port_str = cfg[serverName]["Port"];
    }
    Logger::Init(serverName);
    Logger::SetLevel(cfg[serverName]["LogLevel"]);
    Logger::Info("Server is starting...");

    try {
        auto pool = AsioIOContextPool::GetInstance();
        //将登录数设置为0
        RedisMgr::GetInstance()->InitCount(serverName);
        Defer derfer([serverName]() {
            RedisMgr::GetInstance()->HDel(LOGIN_COUNT, serverName);
            RedisMgr::GetInstance()->Close();
            });

        boost::asio::io_context  io_context;
        auto port_str = cfg[serverName]["Port"];
        //创建Cserver智能指针
        auto handler = std::bind(&LogicSystem::PostTask, LogicSystem::GetInstance().get(), std::placeholders::_1, std::placeholders::_2);
        auto pointer_server = std::make_shared<CServer>(io_context, atoi(port_str.c_str()), handler);
        //定义一个GrpcServer
        std::string server_address(cfg[serverName]["Host"] + ":" + cfg[serverName]["RPCPort"]);
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
