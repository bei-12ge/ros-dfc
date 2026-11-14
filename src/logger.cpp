#include "ros/logger.h"

#include <iostream>

namespace ros_sim {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    closeFileSink();
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return level_;
}

void Logger::enableConsole(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    console_ = enabled;
}

bool Logger::addFileSink(const std::string& path) {
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
        // std::ios::app：追加写，多次运行不会覆盖历史日志
        file_.open(path, std::ios::out | std::ios::app);
        if (file_.is_open()) {
            return true;
        }
    }
    // 文件不可写属于环境问题，不能让业务崩溃，降级为仅控制台并提示
    warn("LOGGER", "日志文件不可写，已降级为仅控制台输出: " + path);
    return false;
}

void Logger::closeFileSink() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool Logger::hasFileSink() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return file_.is_open();
}

bool Logger::shouldLog(LogLevel level) const noexcept {
    return static_cast<int>(level) >= static_cast<int>(level_);
}

void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!shouldLog(level)) {
        return;
    }
    const std::string line =
        "[" + nowTimestamp() + "][" + toString(level) + "][" + module + "] " + message;

    if (console_) {
        // 错误走 std::cerr，便于 shell 重定向区分告警与正常输出
        (level == LogLevel::ERROR ? std::cerr : std::cout) << line << '\n';
    }
    if (file_.is_open()) {
        file_ << line << '\n';
        file_.flush();  // 立即落盘：进程异常退出时也不丢关键日志
    }
}

void Logger::info(const std::string& module, const std::string& message) {
    log(LogLevel::INFO, module, message);
}

void Logger::warn(const std::string& module, const std::string& message) {
    log(LogLevel::WARN, module, message);
}

void Logger::error(const std::string& module, const std::string& message) {
    log(LogLevel::ERROR, module, message);
}

void Logger::logException(const RosException& ex) {
    log(ex.level(), ex.module(), "[" + ex.code() + "] " + ex.what());
}

}  // namespace ros_sim
