#include "ros/subscriber.h"

#include "ros/exception.h"
#include "ros/logger.h"
#include "ros/message.h"
#include "ros/ros.h"
#include "ros/util.h"

namespace ros_sim {

Subscriber::Subscriber(std::string name, std::string topic, Callback callback)
    : Node(std::move(name)), topic_(trim(topic)), callback_(std::move(callback)) {
    if (isBlank(topic_)) {
        throw TopicException::emptyName();
    }
}

void Subscriber::init() {
    if (isRunning()) {
        Logger::instance().warn("SUB", "节点已初始化，忽略重复 init: " + name());
        return;
    }
    ROS::instance().registerSubscriber(*this);
    markRunning(true);
    Logger::instance().info("SUB", "订阅者节点初始化完成: 节点=" + name() + ", 话题=" + topic_);
}

void Subscriber::shutdown() {
    if (!isRunning()) {
        return;
    }
    ROS::instance().unregisterNode(name());
    markRunning(false);
    Logger::instance().info("SUB", "订阅者节点已关闭: 节点=" + name() + ", 话题=" + topic_);
}

void Subscriber::receive(const std::shared_ptr<const Message>& message) {
    if (!message) {
        throw MessageException::nullMessage();
    }
    // FR-03.4：接收日志必须包含消息内容与时间戳
    Logger::instance().info("SUB", "[接收] 节点=" + name() + " 收到消息: 内容=" +
                                        message->content() + ", 时间戳=" + message->timestamp() +
                                        ", 类型=" + message->typeName() + ", 话题=" + topic_);
    received_.fetch_add(1);

    // 回调先取副本、锁外执行：用户回调里可以再次发布消息而不死锁
    Callback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = callback_;
    }
    if (!callback) {
        return;
    }
    try {
        callback(*this, message);
    } catch (const std::exception& ex) {
        // 回调异常必须隔离，否则一条坏消息会打断整个话题的广播
        Logger::instance().error("SUB", "订阅者回调抛出异常，已隔离: 节点=" + name() +
                                            ", 原因=" + ex.what());
    } catch (...) {
        Logger::instance().error("SUB", "订阅者回调抛出未知异常，已隔离: 节点=" + name());
    }
}

void Subscriber::setCallback(Callback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = std::move(callback);
}

}  // namespace ros_sim
