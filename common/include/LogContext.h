#pragma once
#include <string>

class LogContext
{
public:
    static void SetTraceId(const std::string& trace_id);
    static std::string GetTraceId();
    static void Clear();

private:
    static thread_local std::string trace_id_;
};
