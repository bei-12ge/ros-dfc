#include "ros/ros.h"

#include <algorithm>

#include "ros/exception.h"
#include "ros/logger.h"
#include "ros/message.h"
#include "ros/node.h"
#include "ros/publisher.h"
#include "ros/subscriber.h"
#include "ros/util.h"

namespace ros_sim {

ROS& ROS::instance() {
    // C++11 起，函数内静态变量的初始化是线程安全的（magic static）
    static ROS core;
    return core;
}

ROS::~ROS() {
    // 核心析构时不再回调节点 shutdown：此时节点对象多半已销毁，
    // 只做表清理，避免二次访问悬空指针。
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    nodes_.clear();
    publishers_.clear();
    subscribers_.clear();
}

// ---------------------------------------------------------------- 注册
void ROS::registerNodeUnlocked(Node& node) {
    if (nodes_.find(node.name()) != nodes_.end()) {
        throw NodeException::duplicateName(node.name());
    }
    nodes_.emplace(node.name(), &node);
}

void ROS::registerPublisher(Publisher& publisher) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (shutdown_) {
        throw RosException("ROS", "ROS-001", LogLevel::ERROR,
                           "系统已关闭，拒绝注册发布者: " + publisher.name());
    }
    registerNodeUnlocked(publisher);  // 名称唯一性守卫
    auto& slot = publishers_[publisher.topic()];
    if (std::find(slot.begin(), slot.end(), &publisher) == slot.end()) {
        slot.push_back(&publisher);
    }
    Logger::instance().info("ROS", "注册发布者: 节点=" + publisher.name() +
                                     ", 话题=" + publisher.topic() +
                                     ", 该话题发布者数=" + std::to_string(slot.size()));
}

void ROS::registerSubscriber(Subscriber& subscriber) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (shutdown_) {
        throw RosException("ROS", "ROS-001", LogLevel::ERROR,
                           "系统已关闭，拒绝注册订阅者: " + subscriber.name());
    }
    registerNodeUnlocked(subscriber);
    auto& slot = subscribers_[subscriber.topic()];
    if (std::find(slot.begin(), slot.end(), &subscriber) == slot.end()) {
        slot.push_back(&subscriber);
    }
    Logger::instance().info("ROS", "注册订阅者: 节点=" + subscriber.name() +
                                     ", 话题=" + subscriber.topic() +
                                     ", 该话题订阅者数=" + std::to_string(slot.size()));
}

void ROS::unregisterNode(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    nodes_.erase(name);

    // 从两张话题映射中摘除该节点（按指针比较，避免同名对象误删）
    for (auto it = publishers_.begin(); it != publishers_.end();) {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [&name](const Publisher* p) { return p->name() == name; }),
                  vec.end());
        it = vec.empty() ? publishers_.erase(it) : std::next(it);
    }
    for (auto it = subscribers_.begin(); it != subscribers_.end();) {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [&name](const Subscriber* s) { return s->name() == name; }),
                  vec.end());
        it = vec.empty() ? subscribers_.erase(it) : std::next(it);
    }
}

// ---------------------------------------------------------------- 转发
size_t ROS::publish(const std::string& topic, const std::shared_ptr<const Message>& message) {
    if (isBlank(topic)) {
        throw TopicException::emptyName();
    }
    if (!message) {
        throw MessageException::nullMessage();
    }

    std::vector<Subscriber*> targets;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto subIt = subscribers_.find(topic);
        const auto pubIt = publishers_.find(topic);
        if (subIt == subscribers_.end() && pubIt == publishers_.end()) {
            // 话题从未被任何节点声明：属于非法转发
            throw TopicException::notFound(topic);
        }
        if (subIt != subscribers_.end()) {
            targets = subIt->second;  // 关键：先拷副本，锁外分发
        }
    }

    if (targets.empty()) {
        Logger::instance().warn("ROS", "话题当前无订阅者，消息被丢弃: " + topic);
        return 0;
    }
    for (auto* subscriber : targets) {
        subscriber->receive(message);  // 无阻塞等待应答 → 异步语义
    }
    Logger::instance().info("ROS", "消息转发完成: 话题=" + topic + ", 送达订阅者数=" +
                                      std::to_string(targets.size()));
    return targets.size();
}

// ---------------------------------------------------------------- 生命周期
void ROS::shutdownAll() {
    std::vector<Node*> snapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (shutdown_) {
            return;  // 幂等
        }
        shutdown_ = true;
        snapshot.reserve(nodes_.size());
        for (const auto& entry : nodes_) {
            snapshot.push_back(entry.second);
        }
    }
    // 不能在持锁状态下遍历并调用 shutdown（其内部会改表）
    for (auto* node : snapshot) {
        node->shutdown();
    }
    Logger::instance().info("ROS", "系统全局关闭完成，共关闭 " + std::to_string(snapshot.size()) +
                                     " 个节点");
}

void ROS::removeNode(const std::string& name) {
    Node* node = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto it = nodes_.find(name);
        if (it == nodes_.end()) {
            throw NodeException::notFound(name);
        }
        node = it->second;
    }
    node->shutdown();  // 输出关闭日志并从三张表注销
    Logger::instance().info("ROS", "运行时删除节点完成: " + name);
}

void ROS::reset() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    nodes_.clear();
    publishers_.clear();
    subscribers_.clear();
    shutdown_ = false;
}

// ---------------------------------------------------------------- 查询
std::vector<std::string> ROS::nodeNames() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(nodes_.size());
    for (const auto& entry : nodes_) {
        names.push_back(entry.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ROS::topics() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : publishers_) {
        result.push_back(entry.first);
    }
    for (const auto& entry : subscribers_) {
        if (std::find(result.begin(), result.end(), entry.first) == result.end()) {
            result.push_back(entry.first);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ROS::publishersOf(const std::string& topic) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> names;
    const auto it = publishers_.find(topic);
    if (it != publishers_.end()) {
        for (const auto* publisher : it->second) {
            names.push_back(publisher->name());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ROS::subscribersOf(const std::string& topic) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> names;
    const auto it = subscribers_.find(topic);
    if (it != subscribers_.end()) {
        for (const auto* subscriber : it->second) {
            names.push_back(subscriber->name());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

size_t ROS::publisherCount(const std::string& topic) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto it = publishers_.find(topic);
    return it == publishers_.end() ? 0 : it->second.size();
}

size_t ROS::subscriberCount(const std::string& topic) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto it = subscribers_.find(topic);
    return it == subscribers_.end() ? 0 : it->second.size();
}

const Node* ROS::findNode(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto it = nodes_.find(name);
    return it == nodes_.end() ? nullptr : it->second;
}

bool ROS::isShutdown() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return shutdown_;
}

}  // namespace ros_sim
