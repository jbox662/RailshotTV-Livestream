#include "core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace railshot {

namespace {

std::mutex g_logMutex;

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error:   return "ERROR";
  default: return "UNKNOWN";
    }
}

QString logFilePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (dir.isEmpty()) {
        return QStringLiteral("railshot.log");
    }
    const QString logsDir = QDir(dir).filePath(QStringLiteral("logs"));
    QDir().mkpath(logsDir);
    return QDir(logsDir).filePath(QStringLiteral("railshot.log"));
}

} // namespace

void Logger::log(LogLevel level, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm localTime{};
    localtime_s(&localTime, &time);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << " [" << levelName(level) << "] " << message;

    const std::string line = oss.str();

    std::lock_guard lock(g_logMutex);
    std::cerr << line << std::endl;

    QFile file(logFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QString::fromStdString(line) << '\n';
    }
}

} // namespace railshot
