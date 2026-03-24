#include <iostream>
#include <memory>
#include "AsioIOContextPool.h"

AsioIOContextPool::AsioIOContextPool(std::size_t size) :_ioContexts(size),
														_works(size),
														_nextIOContext(0) 
{
	for (std::size_t i = 0; i < size; ++i) 
	{
		_works[i] = std::make_unique<Work>(_ioContexts[i].get_executor());
	}

	//遍历多个ioContext，创建多个线程，每个线程内部启动ioContext
	for (std::size_t i = 0; i < _ioContexts.size(); ++i) 
	{
		_threads.emplace_back(
			[this, i]() {_ioContexts[i].run();}
		);
	}
}

AsioIOContextPool::~AsioIOContextPool() {
	Stop();
	std::cout << "AsioIOContextPool destruct" << std::endl;
}

boost::asio::io_context& AsioIOContextPool::GetIOContext() 
{
	auto& Context = _ioContexts[_nextIOContext++];
	if (_nextIOContext == _ioContexts.size()) 
	{
		_nextIOContext = 0;
	}
	return Context;
}

void AsioIOContextPool::Stop() {
    // 1. 重置所有的 work guard
    //    这表示我们不再人为地阻止 io_context 在没有任务时退出。
    //    reset() 会自动调用 executor 的 on_work_finished()。
    for (auto& work_ptr : _works) {
        if (work_ptr) { // 确保指针有效
            work_ptr.reset(); // 销毁 work guard 对象
        }
    }

    // 2. 停止所有的 io_context
    //    这将导致所有在 run() 中阻塞的线程尽快返回。
    //    挂起的异步操作会被取消（回调函数以 operation_aborted 错误被调用）。
    for (auto& io_ctx : _ioContexts) {
        io_ctx.stop();
    }

    // 3. 等待所有 I/O 线程执行完毕
    for (auto& t : _threads) {
        if (t.joinable()) { // 检查线程是否可以被 join
            t.join();
        }
    }
    // （可选）清空线程容器，如果需要重用 AsioIOContextPool 对象
    _threads.clear();
    // （可选）同样可以清空 _works，虽然 unique_ptr reset 后已经是 nullptr
    _works.clear();
    _works.resize(_ioContexts.size()); // 如果需要重新填充
}