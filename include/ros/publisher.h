#pragma once
// ---------------------------------------------------------------------------
// 发布者节点 Publisher
//   * 继承 Node，扩展"话题名称"属性；
//   * init() 向 ROS 核心注册自身，shutdown() 注销；
//   * publish() 把传感器字符串包装为 Message，输出发布日志后转发给核心；
//   * 发布过程不等待订阅者应答（异步语义）。
// ---------------------------------------------------------------------------
#include <atomic>
#include <memory>
#include <string>

#include "ros/node.h"

namespace ros_sim {

class Message;

class Publisher : public Node {
public:
    /// 空/全空白话题名抛 TopicException(TOPIC-001)
    Publisher(std::string name, std::string topic);

    /// 话题名称（只读）
    const std::string& topic() const noexcept { return topic_; }

    void init() override;
    void shutdown() override;

    /// 核心发布功能：接收传感器字符串 → 创建 Message → 日志 → 转发
    /// @return 实际送达的订阅者数量
    size_t publish(const std::string& sensorData);

    /// 复用已有消息对象（会做一次拷贝）
    size_t publish(const Message& message);

    /// 推荐用法：传共享指针可保留多态（如 SensorMessage），且无拷贝
    size_t publish(const std::shared_ptr<const Message>& message);

    /// 累计发布条数
    size_t publishedCount() const noexcept { return published_.load(); }

private:
    std::string topic_;
    std::atomic<size_t> published_{0};
};

}  // namespace ros_sim
