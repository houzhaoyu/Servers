#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include "AsioIOContextPool.h"
#include "type.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class CServer: public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc, PortType& port);
	void Start();

private:
	net::io_context& _ioc;
	tcp::acceptor _acceptor;
	tcp::socket   _socket;
};


