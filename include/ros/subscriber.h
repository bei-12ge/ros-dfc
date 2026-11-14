#pragma once
// ---------------------------------------------------------------------------
// 订阅者节点 Subscriber
//   * 继承 Node，扩展"话题名称"属性；
//   * receive() 输出包含"消息内容 + 时间戳"的接收日志，完成被动接收；
//   * 支持用户回调（std::function），用于业务扩展；
//   * 回调异常在本地捕获，绝不影响发布者与其他订阅者。
// ---------------------------------------------------------------------------
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "ros/node.h"

namespace ros_sim {

class Message;

class Subscriber : public Node {
public:
    /// 业务回调：第一个参数是订阅者自身，第二个是共享的只读消息
    using Callback =
        std::function<void(const Subscriber&, const std::shared_ptr<const Message>&)>;

    /// 空/全空白话题名抛 TopicException(TOPIC-001)
    Subscriber(std::string name, std::string topic, Callback callback = nullptr);

    /// 话题名称（只读）
    const std::string& topic() const noexcept { return topic_; }

    void init() override;
    void shutdown() override;

    /// 核心订阅功能：接收消息对象并输出接收日志
    void receive(const std::shared_ptr<const Message>& message);

    /// 运行期替换业务回调
    void setCallback(Callback callback);

    /// 累计接收条数
    size_t receivedCount() const noexcept { return received_.load(); }

private:
    std::string topic_;
    Callback callback_;
    mutable std::mutex callbackMutex_;
    std::atomic<size_t> received_{0};
};

}  // namespace ros_sim
