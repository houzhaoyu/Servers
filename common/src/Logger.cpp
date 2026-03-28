#include "Logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::Init(const std::string& server_name)
{
    // 创建目录
    std::filesystem::create_directories("log/" + server_name);

    // ⚠️ 防止重复初始化
    static bool inited = false;
    if (inited) return;
    inited = true;

    // 初始化线程池（只允许一次）
    spdlog::init_thread_pool(8192, 1);

    // sinks
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto info_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/" + server_name + "/info.log",
        10 * 1024 * 1024,
        5
    );

    auto error_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/" + server_name + "/error.log",
        10 * 1024 * 1024,
        5
    );

    console_sink->set_level(spdlog::level::info);
    info_sink->set_level(spdlog::level::info);
    error_sink->set_level(spdlog::level::err);

    std::vector<spdlog::sink_ptr> sinks{ console_sink, info_sink, error_sink };

    logger_ = std::make_shared<spdlog::async_logger>(
        server_name,
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );

    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");
    logger_->set_level(spdlog::level::debug);
    logger_->flush_on(spdlog::level::err);

    spdlog::set_default_logger(logger_);

    // 定时刷盘
    spdlog::flush_every(std::chrono::seconds(3));
}

void Logger::Debug(const std::string& msg)
{
    spdlog::debug("[trace_id={}] {}", LogContext::GetTraceId(), msg);
}

void Logger::Info(const std::string& msg)
{
    spdlog::info("[trace_id={}] {}", LogContext::GetTraceId(), msg);
}

void Logger::Warn(const std::string& msg)
{
    spdlog::warn("[trace_id={}] {}", LogContext::GetTraceId(), msg);
}

void Logger::Error(const std::string& msg)
{
    spdlog::error("[trace_id={}] {}", LogContext::GetTraceId(), msg);
}