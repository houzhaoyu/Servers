#include <iostream>

#include "CServer.h"
#include "ConfigMgr.h"

#include "const.h"
#include "hiredis.h"
#include "Logger.h"

int main()
{
    auto &gCfgMgr = ConfigMgr::Inst();
    std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
    PortType gate_port = atoi(gate_port_str.c_str());
    Logger::Init("GateServer");
    Logger::SetLevel(gCfgMgr["GateServer"]["LogLevel"]);
    Logger::Info("{} is starting...", "GateServer");

    try
    {
        PortType port = static_cast<PortType>(8080);
        net::io_context ioc;
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code &error, int signal_number)
                           {
            if (error)
            {
                return;
            }

            ioc.stop(); });

        std::make_shared<CServer>(ioc, port)->Start();
        Logger::Info("Gate Server listen on port : {}", port);
        ioc.run();
    }
    catch (std::exception const &e)
    {
        Logger::Error("Exception: {}", e.what());
        return EXIT_FAILURE;
    }
}
