#pragma once

#include <string>

namespace railshot {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message) { log(LogLevel::Debug, message); }
    static void info(const std::string& message)  { log(LogLevel::Info, message); }
    static void warn(const std::string& message)  { log(LogLevel::Warning, message); }
    static void error(const std::string& message) { log(LogLevel::Error, message); }
};

} // namespace railshot
