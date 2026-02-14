#pragma once

#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

namespace SH3DS
{
    /**
     * @brief Log levels.
     */
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    /**
     * @brief Type-safe logging wrapper.
     * This class acts as a stateless proxy to hide library-specific headers and types from the public interface.
     */
    class Logger
    {
    public:
        /**
         * @brief Returns the logger instance.
         */
        static Logger &Get();

        /**
         * @brief Sets the logger name. Has no effect if the logger has already been used.
         */
        static void SetLoggerName(const std::string &name);

        /**
         * @brief Trace logging.
         */
        template<typename... Args> void Trace(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Trace, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Debug logging.
         */
        template<typename... Args> void Debug(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Debug, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Info logging.
         */
        template<typename... Args> void Info(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Info, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Warning logging.
         */
        template<typename... Args> void Warn(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Warn, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Error logging.
         */
        template<typename... Args> void Error(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Error, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Critical logging.
         */
        template<typename... Args>
        void Critical(const std::source_location &loc, const std::string &fmt, Args &&...args)
        {
            LogMessage(LogLevel::Critical, loc, std::vformat(fmt, std::make_format_args(args...)));
        }

        /**
         * @brief Flushes the logger.
         */
        void Flush();

        /**
         * @brief Sets the log level.
         */
        void SetLevel(LogLevel level);

    private:
        Logger() = default;

        /**
         * @brief Internal logging implementation.
         */
        void LogMessage(LogLevel level, const std::source_location &loc, std::string_view message);
    };

} // namespace SH3DS

/**
 * @brief Logging macros.
 */
#define LOG_TRACE(...) ::SH3DS::Logger::Get().Trace(std::source_location::current(), __VA_ARGS__)
#define LOG_DEBUG(...) ::SH3DS::Logger::Get().Debug(std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...) ::SH3DS::Logger::Get().Info(std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(...) ::SH3DS::Logger::Get().Warn(std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) ::SH3DS::Logger::Get().Error(std::source_location::current(), __VA_ARGS__)
#define LOG_CRITICAL(...) ::SH3DS::Logger::Get().Critical(std::source_location::current(), __VA_ARGS__)
