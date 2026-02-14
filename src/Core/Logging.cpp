#include "Logging.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace
{
    std::string &GetConfiguredName()
    {
        static std::string loggerName = "SH3DS";
        return loggerName;
    }

    std::shared_ptr<spdlog::logger> &GetInternalLogger()
    {
        static auto logger = spdlog::stdout_color_mt(GetConfiguredName());
        return logger;
    }

    std::string_view ExtractFileName(const std::source_location &loc)
    {
        const char *path = loc.file_name();
        const char *filename = path;

        for (const char *p = path; *p; ++p)
        {
            if (*p == '/' || *p == '\\')
            {
                filename = p + 1;
            }
        }
        return filename;
    }
} // namespace

namespace SH3DS
{
    Logger &Logger::Get()
    {
        static Logger instance;
        return instance;
    }

    void Logger::SetLoggerName(const std::string &name)
    {
        GetConfiguredName() = name;
    }

    void Logger::LogMessage(LogLevel level, const std::source_location &loc, std::string_view message)
    {
        spdlog::level::level_enum spdLevel;
        switch (level)
        {
        case LogLevel::Trace:
            spdLevel = spdlog::level::trace;
            break;
        case LogLevel::Debug:
            spdLevel = spdlog::level::debug;
            break;
        case LogLevel::Info:
            spdLevel = spdlog::level::info;
            break;
        case LogLevel::Warn:
            spdLevel = spdlog::level::warn;
            break;
        case LogLevel::Error:
            spdLevel = spdlog::level::err;
            break;
        case LogLevel::Critical:
            spdLevel = spdlog::level::critical;
            break;
        default:
            spdLevel = spdlog::level::info;
        }

        GetInternalLogger()->log(spdLevel, "[{0}:{1}] {2}", ExtractFileName(loc), loc.line(), message);
    }

    void Logger::Flush()
    {
        GetInternalLogger()->flush();
    }

    void Logger::SetLevel(LogLevel level)
    {
        spdlog::level::level_enum spdLevel;
        switch (level)
        {
        case LogLevel::Trace:
            spdLevel = spdlog::level::trace;
            break;
        case LogLevel::Debug:
            spdLevel = spdlog::level::debug;
            break;
        case LogLevel::Info:
            spdLevel = spdlog::level::info;
            break;
        case LogLevel::Warn:
            spdLevel = spdlog::level::warn;
            break;
        case LogLevel::Error:
            spdLevel = spdlog::level::err;
            break;
        case LogLevel::Critical:
            spdLevel = spdlog::level::critical;
            break;
        default:
            spdLevel = spdlog::level::info;
        }
        GetInternalLogger()->set_level(spdLevel);
    }

} // namespace SH3DS
