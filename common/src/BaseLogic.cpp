#include "BaseLogic.h"
#include "Logger.h"

// 构造函数现在只初始化基础变量，不启动线程
BaseLogic::BaseLogic(int thread_count) : _b_stop(false) {
    for (int i = 0; i < thread_count; ++i) {
        _workers.push_back(std::make_unique<Worker>());
    }
}

BaseLogic::~BaseLogic() {
    _b_stop = true;
    for (auto& w : _workers) {
        w->cv.notify_all();
        if (w->thread.joinable()) w->thread.join();
    }
}

// 提供一个显式的启动接口
void BaseLogic::Start() {
    for (size_t i = 0; i < _workers.size(); ++i) {
        _workers[i]->thread = std::thread(&BaseLogic::WorkerLoop, this, i);
    }
}

void BaseLogic::PostTask(std::shared_ptr<LogicTask> task, const std::string& key) {
    Logger::Debug("PostTask : msgId = {}", task->recvnode->_msg_id);
    size_t worker_idx = 0;
    if (!key.empty()) {
        worker_idx = std::hash<std::string>{}(key) % _workers.size();
    }

    auto& w = _workers[worker_idx];
    {
        std::lock_guard<std::mutex> lock(w->mtx);
        w->queue.push(task);
    }
    w->cv.notify_one();
}

void BaseLogic::WorkerLoop(int index) {
    auto& w = _workers[index];
    while (!_b_stop) {
        std::unique_lock<std::mutex> lock(w->mtx);
        w->cv.wait(lock, [&] { return !w->queue.empty() || _b_stop; });

        if (_b_stop && w->queue.empty()) break;

        auto task = w->queue.front();
        w->queue.pop();
        lock.unlock();

        auto it = _handlers.find(task->recvnode->_msg_id);
        if (it != _handlers.end()) {
            it->second(task->session, it->first,
                std::string(task->recvnode->_data, task->recvnode->_cur_len));
        }
    }
}
