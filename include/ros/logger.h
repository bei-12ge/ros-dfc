#pragma once
// ---------------------------------------------------------------------------
// 分级日志 Logger（单例）
//   行格式：[YYYY-MM-DD HH:MM:SS][LEVEL][MODULE] message
//   输出目标：控制台（std::cout）+ 可选文件（文本，供加分项"日志持久化"）
//   线程安全：内部互斥量保护写操作；文件打开失败不中断业务，降级为仅控制台。
// ---------------------------------------------------------------------------
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "ros/exception.h"
#include "ros/timestamp.h"

namespace ros_sim {

class Logger {
public:
    /// 全局唯一实例（Meyers Singleton，C++11 起线程安全）
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /// 设置级别下限：低于该级别的日志被丢弃（默认 INFO）
    void setLevel(LogLevel level);
    LogLevel level() const;

    /// 控制台开关（默认开）
    void enableConsole(bool enabled);

    /// 打开文件输出。成功返回 true；失败返回 false（同时记一条 WARN，不抛异常）
    bool addFileSink(const std::string& path);
    /// 关闭并刷新文件输出
    void closeFileSink();
    /// 是否已挂载文件输出
    bool hasFileSink() const;

    /// 开启/关闭内存捕获（供运行可视化报告使用，默认关闭）
    void enableCapture(bool enabled);
    /// 是否正在捕获
    bool capturing() const;
    /// 取出捕获到的原始日志行（拷贝，线程安全）
    std::vector<std::string> capturedLines() const;
    /// 清空捕获缓冲
    void clearCapture();

    /// 通用写日志入口
    void log(LogLevel level, const std::string& module, const std::string& message);

    void info(const std::string& module, const std::string& message);
    void warn(const std::string& module, const std::string& message);
    void error(const std::string& module, const std::string& message);

    /// 按异常自带的级别与模块输出，格式统一为 [MODULE][CODE] message
    void logException(const RosException& ex);

private:
    Logger();
    ~Logger();

    bool shouldLog(LogLevel level) const noexcept;

    mutable std::recursive_mutex mutex_;
    LogLevel level_ = LogLevel::INFO;
    bool console_ = true;
    std::ofstream file_;

    bool capture_ = false;
    std::vector<std::string> captured_;
    static constexpr size_t kMaxCapturedLines = 8192;  // 上限，防止长时间运行占满内存
};

}  // namespace ros_sim
