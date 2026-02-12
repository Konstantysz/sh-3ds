#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <format>
#include <memory>
#include <source_location>

namespace SH3DS
{
    /**
     * @brief Type-safe logging wrapper around spdlog.
     */
    class Logger
    {
    public:
        /**
         * @brief Returns the logger instance.
         * @return Logger instance
         */
        static Logger &Get()
        {
            static Logger instance(spdlog::stdout_color_mt(GetLoggerName()));
            return instance;
        }

        /**
         * @brief Sets the logger name (must be called before first Get()).
         * @param name Logger name to display in logs
         */
        static void SetLoggerName(const std::string &name)
        {
            GetLoggerName() = name;
        }

    private:
        /**
         * @brief Returns the logger name (default: "SH3DS").
         * @return Logger name
         */
        static std::string &GetLoggerName()
        {
            static std::string loggerName = "SH3DS";
            return loggerName;
        }

    public:
        /**
         * @brief Trace logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args>
        void Trace(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->trace(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Debug logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args>
        void Debug(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->debug(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Info logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args> void Info(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->info(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Warning logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args> void Warn(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->warn(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Error logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args>
        void Error(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->error(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Critical logging.
         * @tparam Args Format argument types
         * @param loc Source location
         * @param format Format string
         * @param args Format arguments
         */
        template<typename... Args>
        void Critical(const std::source_location &loc, const std::string &format, Args &&...args)
        {
            logger_->critical(
                "[{}:{}] {}", GetFileName(loc), loc.line(), std::vformat(format, std::make_format_args(args...)));
        }

        /**
         * @brief Returns the spdlog instance.
         * @return spdlog logger
         */
        spdlog::logger &GetSpdlog()
        {
            return *logger_;
        }

        const spdlog::logger &GetSpdlog() const
        {
            return *logger_;
        }

        /**
         * @brief Flushes the logger.
         */
        void Flush()
        {
            logger_->flush();
        }

        /**
         * @brief Sets the log level.
         * @param level Log level
         */
        void SetLevel(spdlog::level::level_enum level)
        {
            logger_->set_level(level);
        }

    private:
        /**
         * @brief Constructs logger.
         * @param spdlogger spdlog logger instance
         */
        explicit Logger(std::shared_ptr<spdlog::logger> spdlogger) : logger_(std::move(spdlogger))
        {
        }

        /**
         * @brief Extracts filename from full path.
         * @param loc Source location
         * @return Filename without path
         */
        static std::string_view GetFileName(const std::source_location &loc)
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

        std::shared_ptr<spdlog::logger> logger_;
    };

} // namespace SH3DS

/**
 * @brief Trace logging macro.
 * @param ... Format string and arguments
 */
#define LOG_TRACE(...) ::SH3DS::Logger::Get().Trace(std::source_location::current(), __VA_ARGS__)

/**
 * @brief Debug logging macro.
 * @param ... Format string and arguments
 */
#define LOG_DEBUG(...) ::SH3DS::Logger::Get().Debug(std::source_location::current(), __VA_ARGS__)

/**
 * @brief Info logging macro.
 * @param ... Format string and arguments
 */
#define LOG_INFO(...) ::SH3DS::Logger::Get().Info(std::source_location::current(), __VA_ARGS__)

/**
 * @brief Warning logging macro.
 * @param ... Format string and arguments
 */
#define LOG_WARN(...) ::SH3DS::Logger::Get().Warn(std::source_location::current(), __VA_ARGS__)

/**
 * @brief Error logging macro.
 * @param ... Format string and arguments
 */
#define LOG_ERROR(...) ::SH3DS::Logger::Get().Error(std::source_location::current(), __VA_ARGS__)

/**
 * @brief Critical logging macro.
 * @param ... Format string and arguments
 */
#define LOG_CRITICAL(...) ::SH3DS::Logger::Get().Critical(std::source_location::current(), __VA_ARGS__)
