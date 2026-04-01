#pragma once
#include <memory>
#include <mutex>
#include <iostream>

template <typename T>
class Singleton
{
protected:
    // 构造函数设置为保护，类外不能构造，但是子类可以
    Singleton() = default;
    Singleton(const Singleton<T> &) = delete;
    Singleton<T> &operator=(const Singleton<T> &st) = delete;
    static std::shared_ptr<T> _instance;

public:
    static std::shared_ptr<T> GetInstance()
    {
        // s_flag在第一次调用GetInstance时初始化
        // once_flag和call_once可以保证线程安全，保证某个函数只被调用一次
        static std::once_flag s_flag;
        // call_once首先检查s_flag是否被设置，如果没有则调用第二个参数，并且设置s_flag
        std::call_once(s_flag, [&]()
                       {
            //使用new构造智能指针，而不是用make_shared
            //因为make_shared要访问构造函数，而构造函数是保护的，无法被外界访问
            _instance = std::shared_ptr<T>(new T); });
        return _instance;
    }

    void PrintAddress()
    {
        // Logger::Info("Singleton address: {}", _instance.get());
    }

    ~Singleton()
    {
        // std::cout << "this is singleton destruct" << std::endl;
    }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
