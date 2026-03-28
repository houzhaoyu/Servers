#include "Logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::Init(const std::string& server_name)
{
    std::filesystem::create_directories("log/" + server_name);

    static bool inited = false;
    if (inited) return;
    inited = true;

    spdlog::init_thread_pool(8192, 1);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto info_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/" + server_name + "/info.log", 10 * 1024 * 1024, 5
    );
    auto error_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/" + server_name + "/error.log", 10 * 1024 * 1024, 5
    );

    // --- 统一 Sink 级别策略 ---
    // 为了让总闸 SetLevel 生效，我们将负责输出的 Sink 级别设为 trace (全开)
    // 这样日志是否输出就只取决于 logger_->set_level()
    console_sink->set_level(spdlog::level::trace);
    info_sink->set_level(spdlog::level::trace);

    // 唯独 error_sink 建议保持 err，因为它是一个专门存错误的文件库
    error_sink->set_level(spdlog::level::err);

    std::vector<spdlog::sink_ptr> sinks{ console_sink, info_sink, error_sink };

    logger_ = std::make_shared<spdlog::async_logger>(
        server_name,
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );

    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");

    // 设置初始默认级别
    logger_->set_level(spdlog::level::info);
    logger_->flush_on(spdlog::level::err);

    spdlog::set_default_logger(logger_);
    spdlog::flush_every(std::chrono::seconds(3));
}

void Logger::SetLevel(const std::string& level_str) {
    if (!logger_) return;

    spdlog::level::level_enum lv = spdlog::level::from_str(level_str);

    // 1. 设置 Logger 总开关
    logger_->set_level(lv);

    // 2. 遍历所有 Sinks，统一它们的门槛
    // 这样当你把级别调低（如 debug）时，Sink 不会拦截这些消息
    for (auto& sink : logger_->sinks()) {
        // 特殊处理：我们通常不希望 error.log 混入 debug 信息
        // 所以如果该 sink 不是 error_sink，就更新它的级别
        // 简单做法：如果是控制台或者 info 文件，则跟随总开关
        // 这里为了彻底“统一”，我们可以全部设置，除了 error 文件建议逻辑保留
        sink->set_level(lv);
    }

    // 如果是开发环境，可以把错误日志的 Sink 强制调回 err
    // 从而保证 error.log 的纯净（可选）
    logger_->sinks().back()->set_level(spdlog::level::err);

    spdlog::info("Log level changed to: {}", level_str);
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