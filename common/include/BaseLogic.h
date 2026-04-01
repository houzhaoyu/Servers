#pragma once
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include "MsgNode.h"

class BaseLogic {
public:
    // 构造时指定线程数
    BaseLogic(int thread_count = 1);
    virtual ~BaseLogic();

    // 统一的投递接口：如果 key 为空则随机/轮询，如果有 key 则哈希
    void PostTask(std::shared_ptr<LogicTask> task, const std::string& key = "");
    void Start();

protected:
    virtual void RegisterHandlers() = 0;
    std::map<MsgIdType, LogicHandler> _handlers;

private:
    // 内部工作单元
    struct Worker {
        std::thread thread;
        std::queue<std::shared_ptr<LogicTask>> queue;
        std::mutex mtx;
        std::condition_variable cv;
    };

    void WorkerLoop(int index);

    std::vector<std::unique_ptr<Worker>> _workers;
    std::atomic<bool> _b_stop;
};
