#include "LogContext.h"

thread_local std::string LogContext::trace_id_;

void LogContext::SetTraceId(const std::string& trace_id)
{
    trace_id_ = trace_id;
}

std::string LogContext::GetTraceId()
{
    return trace_id_;
}

void LogContext::Clear()
{
    trace_id_.clear();
}
