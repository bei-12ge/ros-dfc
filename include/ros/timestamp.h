#pragma once
// ---------------------------------------------------------------------------
// 时间工具：统一提供 "YYYY-MM-DD HH:MM:SS" 的本地时间格式化能力。
// 说明：
//   1. std::localtime 非线程安全，这里在 POSIX 上用 localtime_r、Windows 上用
//      localtime_s，保证多线程下发消息时的安全性；
//   2. 采用 inline 实现，避免为一个两行的函数新增 .cpp 与链接依赖。
// ---------------------------------------------------------------------------
#include <cstdio>
#include <ctime>
#include <string>

namespace ros_sim {

namespace detail {

inline std::tm localTime(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

}  // namespace detail

/// 将时间点格式化为 "YYYY-MM-DD HH:MM:SS"（本地时区）
inline std::string formatTimestamp(std::time_t t) {
    const std::tm tm = detail::localTime(t);
    char buffer[32];
    // 用 strftime 而非 snprintf：格式固定为 19 字符，且不会触发 -Wformat-truncation
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

/// 当前本地时间戳
inline std::string nowTimestamp() {
    return formatTimestamp(std::time(nullptr));
}

}  // namespace ros_sim
