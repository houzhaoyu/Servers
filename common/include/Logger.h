#pragma once
#include <memory>
#include <string>
#include <utility>

#include "spdlog/spdlog.h"
#include "LogContext.h"

class Logger
{
public:
    static void Init(const std::string& server_name);

    // 普通接口
    static void Debug(const std::string& msg);
    static void Info(const std::string& msg);
    static void Warn(const std::string& msg);
    static void Error(const std::string& msg);

    // 格式化接口
    template<typename... Args>
    static void Info(const char* fmt, Args&&... args)
    {
        Log(spdlog::level::info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(const char* fmt, Args&&... args)
    {
        Log(spdlog::level::err, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(const char* fmt, Args&&... args)
    {
        Log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(const char* fmt, Args&&... args)
    {
        Log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
    }

private:
    // ⚠️ 模板实现必须在头文件
    template<typename... Args>
    static void Log(spdlog::level::level_enum level,
        const char* fmt, Args&&... args)
    {
        if (!logger_) return;

        std::string trace_id = LogContext::GetTraceId();

        // 拼接 trace_id
        std::string new_fmt = "[trace_id={}] ";
        new_fmt += fmt;

        logger_->log(level, new_fmt, trace_id, std::forward<Args>(args)...);
    }

private:
    static std::shared_ptr<spdlog::logger> logger_;
};