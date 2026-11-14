#include "ros/exception.h"

namespace ros_sim {

const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------- 基类
RosException::RosException(std::string module, std::string code, LogLevel level,
                           const std::string& message)
    : std::runtime_error(message),
      module_(std::move(module)),
      code_(std::move(code)),
      level_(level) {}

std::string RosException::toLogString() const {
    return "[" + module_ + "][" + code_ + "] " + what();
}

// ---------------------------------------------------------------- 节点
NodeException::NodeException(std::string code, const std::string& message, LogLevel level)
    : RosException("NODE", std::move(code), level, message) {}

NodeException NodeException::emptyName() {
    return NodeException("NODE-001", "节点名称不能为空或全为空白字符");
}

NodeException NodeException::duplicateName(const std::string& name) {
    return NodeException("NODE-002", "节点名称重复注册: " + name);
}

NodeException NodeException::notFound(const std::string& name) {
    return NodeException("NODE-003", "节点不存在或已关闭: " + name, LogLevel::WARN);
}

// ---------------------------------------------------------------- 话题
TopicException::TopicException(std::string code, const std::string& message, LogLevel level)
    : RosException("TOPIC", std::move(code), level, message) {}

TopicException TopicException::emptyName() {
    return TopicException("TOPIC-001", "话题名称不能为空或全为空白字符");
}

TopicException TopicException::notFound(const std::string& topic) {
    return TopicException("TOPIC-002", "话题不存在，无法转发消息: " + topic, LogLevel::WARN);
}

// ---------------------------------------------------------------- 消息
MessageException::MessageException(std::string code, const std::string& message, LogLevel level)
    : RosException("MSG", std::move(code), level, message) {}

MessageException MessageException::emptyContent() {
    return MessageException("MSG-001", "消息内容不能为空或全为空白字符");
}

MessageException MessageException::nullMessage() {
    return MessageException("MSG-002", "消息对象为空指针，无法转发");
}

MessageException MessageException::invalidSensorValue(const std::string& field, double value) {
    return MessageException("MSG-003", "传感器数值超出合理量程: " + field + "=" +
                                            std::to_string(value));
}

// ---------------------------------------------------------------- 服务
ServiceException::ServiceException(std::string code, const std::string& message, LogLevel level)
    : RosException("SERVICE", std::move(code), level, message) {}

ServiceException ServiceException::notFound(const std::string& service) {
    return ServiceException("SERVICE-001", "服务未注册: " + service, LogLevel::WARN);
}

ServiceException ServiceException::handlerFailed(const std::string& service,
                                                 const std::string& reason) {
    return ServiceException("SERVICE-002", "服务处理失败 [" + service + "]: " + reason);
}

ServiceException ServiceException::clientNotRunning(const std::string& client) {
    return ServiceException("SERVICE-003", "客户端未初始化即调用服务: " + client);
}

ServiceException ServiceException::emptyName() {
    return ServiceException("SERVICE-004", "服务名称不能为空或全为空白字符");
}

ServiceException ServiceException::emptyHandler(const std::string& service) {
    return ServiceException("SERVICE-005", "服务处理函数为空: " + service);
}

ServiceException ServiceException::duplicateService(const std::string& service) {
    return ServiceException("SERVICE-006", "服务名称重复注册: " + service);
}

}  // namespace ros_sim
