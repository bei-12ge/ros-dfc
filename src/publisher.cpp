#include "ros/publisher.h"

#include "ros/exception.h"
#include "ros/logger.h"
#include "ros/message.h"
#include "ros/ros.h"
#include "ros/util.h"

namespace ros_sim {

Publisher::Publisher(std::string name, std::string topic)
    : Node(std::move(name)), topic_(trim(topic)) {
    if (isBlank(topic_)) {
        throw TopicException::emptyName();
    }
}

void Publisher::init() {
    if (isRunning()) {
        Logger::instance().warn("PUB", "节点已初始化，忽略重复 init: " + name());
        return;
    }
    // 先注册（可能抛 NODE-002），成功后再标记运行，避免"半初始化"状态
    ROS::instance().registerPublisher(*this);
    markRunning(true);
    Logger::instance().info("PUB", "发布者节点初始化完成: 节点=" + name() + ", 话题=" + topic_);
}

void Publisher::shutdown() {
    if (!isRunning()) {
        return;  // 幂等：重复关闭不产生重复日志、不抛异常
    }
    ROS::instance().unregisterNode(name());
    markRunning(false);
    Logger::instance().info("PUB", "发布者节点已关闭: 节点=" + name() + ", 话题=" + topic_);
}

size_t Publisher::publish(const std::string& sensorData) {
    if (!isRunning()) {
        throw NodeException::notFound(name());
    }
    // 模拟"接收传感器字符串信息 → 创建消息对象"，空内容由 Message 构造函数拦截
    return publish(std::make_shared<const Message>(sensorData));
}

size_t Publisher::publish(const Message& message) {
    return publish(std::make_shared<const Message>(message));
}

size_t Publisher::publish(const std::shared_ptr<const Message>& message) {
    if (!isRunning()) {
        throw NodeException::notFound(name());
    }
    if (!message) {
        throw MessageException::nullMessage();
    }
    Logger::instance().info("PUB", "发布消息: 节点=" + name() + ", 话题=" + topic_ +
                                        ", " + message->toString());
    const size_t delivered = ROS::instance().publish(topic_, message);
    published_.fetch_add(1);
    return delivered;
}

}  // namespace ros_sim
