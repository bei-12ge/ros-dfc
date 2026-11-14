#pragma once
// ---------------------------------------------------------------------------
// 字符串小工具（header-only）
// ---------------------------------------------------------------------------
#include <cctype>
#include <string>

namespace ros_sim {

/// 判断字符串是否为空或仅由空白字符（空格/Tab/换行）组成
inline bool isBlank(const std::string& s) {
    for (const char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

/// 去除首尾空白，用于日志展示
inline std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace ros_sim
