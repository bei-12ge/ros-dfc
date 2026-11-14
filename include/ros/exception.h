#pragma once
// ---------------------------------------------------------------------------
// 异常体系
//   RosException（基类，继承 std::runtime_error）
//     ├── NodeException     节点相关：命名、注册、生命周期      NODE-xxx
//     ├── TopicException    话题相关：命名、路由                TOPIC-xxx
//     ├── MessageException  消息相关：内容、空对象              MSG-xxx
//     └── ServiceException  服务相关：未注册、处理失败          SERVICE-xxx
//
// 设计要点：
//   * 每个异常携带 module（模块）、code（错误码）、level（日志级别）三要素，
//     既能被顶层统一捕获，也能被日志系统分级输出；
//   * 继承 std::runtime_error，因此 catch (const std::exception&) 也能兜住，
//     兼容标准库与第三方代码的异常处理习惯；
//   * 具体错误用静态工厂方法构造（如 NodeException::emptyName()），
//     避免调用方硬编码错误码字符串。
// ---------------------------------------------------------------------------
#include <stdexcept>
#include <string>

namespace ros_sim {

/// 日志级别：数值越大越严重，用于级别过滤
enum class LogLevel { INFO = 0, WARN = 1, ERROR = 2 };

/// 级别转字符串（INFO / WARN / ERROR）
const char* toString(LogLevel level);

// ---------------------------------------------------------------------------
// 异常基类
// ---------------------------------------------------------------------------
class RosException : public std::runtime_error {
public:
    RosException(std::string module, std::string code, LogLevel level, const std::string& message);

    /// 模块名，如 "NODE"
    const std::string& module() const noexcept { return module_; }
    /// 错误码，如 "NODE-001"
    const std::string& code() const noexcept { return code_; }
    /// 建议的日志级别
    LogLevel level() const noexcept { return level_; }

    /// 统一日志串："[NODE][NODE-001] 节点名称不能为空"
    std::string toLogString() const;

private:
    std::string module_;
    std::string code_;
    LogLevel level_;
};

// ---------------------------------------------------------------------------
// 节点异常
// ---------------------------------------------------------------------------
class NodeException : public RosException {
public:
    NodeException(std::string code, const std::string& message,
                  LogLevel level = LogLevel::ERROR);

    /// NODE-001 节点名称为空/全空白（ERROR，编程错误）
    static NodeException emptyName();
    /// NODE-002 节点名称重复注册（ERROR，违反名称唯一约束）
    static NodeException duplicateName(const std::string& name);
    /// NODE-003 节点不存在或已关闭（WARN，可降级处理）
    static NodeException notFound(const std::string& name);
};

// ---------------------------------------------------------------------------
// 话题异常
// ---------------------------------------------------------------------------
class TopicException : public RosException {
public:
    TopicException(std::string code, const std::string& message,
                   LogLevel level = LogLevel::ERROR);

    /// TOPIC-001 话题名称为空（ERROR）
    static TopicException emptyName();
    /// TOPIC-002 话题不存在（WARN：可能是发布时序问题，不致命）
    static TopicException notFound(const std::string& topic);
};

// ---------------------------------------------------------------------------
// 消息异常
// ---------------------------------------------------------------------------
class MessageException : public RosException {
public:
    MessageException(std::string code, const std::string& message,
                     LogLevel level = LogLevel::ERROR);

    /// MSG-001 消息内容为空/全空白（ERROR）
    static MessageException emptyContent();
    /// MSG-002 消息对象为空指针（ERROR）
    static MessageException nullMessage();
    /// MSG-003 传感器数值超出合理量程（ERROR）
    static MessageException invalidSensorValue(const std::string& field, double value);
};

// ---------------------------------------------------------------------------
// 服务异常
// ---------------------------------------------------------------------------
class ServiceException : public RosException {
public:
    ServiceException(std::string code, const std::string& message,
                     LogLevel level = LogLevel::ERROR);

    /// SERVICE-001 服务未注册（WARN）
    static ServiceException notFound(const std::string& service);
    /// SERVICE-002 服务处理函数抛出异常（ERROR）
    static ServiceException handlerFailed(const std::string& service, const std::string& reason);
    /// SERVICE-003 客户端未初始化即调用（ERROR）
    static ServiceException clientNotRunning(const std::string& client);
    /// SERVICE-004 服务名称为空（ERROR）
    static ServiceException emptyName();
    /// SERVICE-005 服务处理函数为空（ERROR）
    static ServiceException emptyHandler(const std::string& service);
    /// SERVICE-006 服务名重复注册（ERROR）
    static ServiceException duplicateService(const std::string& service);
};

}  // namespace ros_sim
