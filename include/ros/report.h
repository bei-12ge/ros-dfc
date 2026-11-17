#pragma once
// ---------------------------------------------------------------------------
// 运行可视化报告（加分展示项）
//   把一次运行期间的日志文本渲染成一个自包含的 HTML 页面：
//   左侧事件流（可按级别筛选、搜索）、右侧节点拓扑与模块分布、底部异常清单。
//   不引入任何第三方依赖：只依赖标准库与 Logger 的内存捕获能力。
// ---------------------------------------------------------------------------
#include <string>
#include <vector>

namespace ros_sim {

namespace report {

/// 解析后的一行日志
struct LogLine {
    std::string time;     // 2026-11-15 20:38:01
    std::string level;    // INFO / WARN / ERROR
    std::string module;   // ROS / PUB / SUB / SERVICE / CALLBACK ...
    std::string code;     // 错误码，例如 NODE-001；无则为空
    std::string message;  // 正文（已剥离 [CODE] 前缀）
};

/// 解析一行原始日志，格式不符时返回 false
bool parseLine(const std::string& raw, LogLine& out);

/// 从原始日志行生成自包含 HTML 报告；成功写盘返回 true
/// htmlPath 为输出文件路径，title 为页面标题，rawLines 为 Logger 捕获到的原始行
bool writeHtml(const std::string& htmlPath,
               const std::vector<std::string>& rawLines,
               const std::string& title);

}  // namespace report

}  // namespace ros_sim
