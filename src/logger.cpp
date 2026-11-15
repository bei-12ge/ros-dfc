#include "ros/logger.h"

#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// windows.h 里定义了同名宏 ERROR（值为 0），会把本文件后面所有
// LogLevel::ERROR 直接替换成 LogLevel::0，导致编译报
// "expected unqualified-id before numeric constant"。
// 因此包含之后立刻取消这个宏，只保留我们自定义枚举里的 ERROR。
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace ros_sim {

Logger::Logger() {
#ifdef _WIN32
    // Windows 控制台默认按本地代码页（简体中文为 GBK/936）解码，
    // 而源文件与日志内容都是 UTF-8，不切换代码页中文会整段乱码。
    // 这里把控制台输入、输出代码页统一设为 UTF-8。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

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
        // 错误走 std::cerr，便于 shell 重定向区分告警与正常输出。
        // 两条流各自缓冲，若不逐行刷新，Windows 下 ERROR 会抢先打印、
        // 与正常输出交错错位；因此写之前先冲掉 cout，写完立即 flush。
        if (level == LogLevel::ERROR) {
            std::cout.flush();
            std::cerr << line << '\n';
            std::cerr.flush();
        } else {
            std::cout << line << '\n';
            std::cout.flush();
        }
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
