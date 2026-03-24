#include "CServer.h"
#include "HttpConnection.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port):_ioc(ioc),
_acceptor(_ioc, tcp::endpoint(tcp::v4(), port)), _socket(ioc)
{

}

void CServer::Start()
{
	auto self = shared_from_this();
	auto& io_context = AsioIOContextPool::GetInstance()->GetIOContext();
	std::shared_ptr<HttpConnection> new_conn = std::make_shared<HttpConnection>(io_context);
	_acceptor.async_accept(new_conn->GetSocket(), [self, new_conn](beast::error_code ec) {
		try {
			//出错则放弃该连接，继续监听其他连接
			if (ec)
			{
				self->Start();
				return;
			}

			new_conn->Start();
			//创建连接，并创建HttpConnection类管理该连接
			//std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
			//HttpConnection(std::move(_socket));

			//继续监听
			self->Start();
		}
		catch (std::exception& exp) {
			std::cout << "exception is " << exp.what() << std::endl;
			self->Start();
		}
		});
}