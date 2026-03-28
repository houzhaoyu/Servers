#pragma once

#include "const.h"

//HTTP服务网络层
class HttpConnection: public std::enable_shared_from_this<HttpConnection>
{
public:
	friend class LogicSystem;
	HttpConnection(tcp::socket socket);
	HttpConnection(boost::asio::io_context&);
	void Start();
	tcp::socket& GetSocket();

private:
	//使用Tcp实现http，需要实现定时检测的功能
	void CheckDeadline();
	void WriteResponse();
	void PreParseGetParam();
	void HandleReq();

	tcp::socket _socket;
	//8KB的缓冲区
	beast::flat_buffer _buffer{ 8192 };
	http::request<http::dynamic_body> _request;
	http::response<http::dynamic_body> _response;
	net::steady_timer deadline_{
		_socket.get_executor(), std::chrono::seconds(60)
	};

	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_params;
};


