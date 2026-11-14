#pragma once
// ---------------------------------------------------------------------------
// ROS 核心管理模块（单例）
//   * 全局唯一实例，禁止拷贝；
//   * 维护 "话题 → 发布者" "话题 → 订阅者" "节点名 → 节点" 三类映射；
//   * 提供注册 / 注销 / 消息转发 / 全局关闭 / 运行时删除节点；
//   * 线程安全：互斥量只保护映射表读写，消息分发在锁外执行（避免持锁回调）。
// ---------------------------------------------------------------------------
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ros_sim {

class Message;
class Node;
class Publisher;
class Subscriber;
class ServiceServer;

class ROS {
public:
    /// 全局唯一实例
    static ROS& instance();

    ROS(const ROS&) = delete;
    ROS& operator=(const ROS&) = delete;

    // ------------------------------------------------------ 注册 / 注销
    /// 注册发布者：建立 节点名→节点、话题→发布者 两条映射
    void registerPublisher(Publisher& publisher);
    /// 注册订阅者：建立 节点名→节点、话题→订阅者 两条映射
    void registerSubscriber(Subscriber& subscriber);
    /// 注册服务端：建立 服务名→服务端 映射（服务名全局唯一）
    void registerService(ServiceServer& service);
    /// 按名称注销任意节点（同时清理全部映射表中的痕迹）；幂等
    void unregisterNode(const std::string& name);

    // ------------------------------------------------------ 消息转发
    /// 将消息推送给该话题下的所有订阅者
    /// @return 送达的订阅者数量；话题不存在抛 TopicException(TOPIC-002)
    size_t publish(const std::string& topic, const std::shared_ptr<const Message>& message);

    // ------------------------------------------------------ 生命周期
    /// 遍历所有节点执行 shutdown()，完成全局关闭（幂等）
    void shutdownAll();
    /// 同步调用服务：阻塞直到服务端返回（FR-07.2）
    std::string callService(const std::string& serviceName, const std::string& request);
    /// 运行时删除单个节点（FR-07.3）
    void removeNode(const std::string& name);
    /// 清空所有映射（仅供测试隔离使用，不触发节点关闭日志）
    void reset();

    // ------------------------------------------------------ 查询
    std::vector<std::string> nodeNames() const;
    std::vector<std::string> topics() const;
    std::vector<std::string> publishersOf(const std::string& topic) const;
    std::vector<std::string> subscribersOf(const std::string& topic) const;
    size_t publisherCount(const std::string& topic) const;
    size_t subscriberCount(const std::string& topic) const;
    std::vector<std::string> serviceNames() const;
    size_t serviceCount() const;
    const ServiceServer* findService(const std::string& serviceName) const;
    const Node* findNode(const std::string& name) const;
    bool isShutdown() const;

private:
    ROS() = default;
    ~ROS();

    /// 调用方须已持锁：校验名称唯一性并写入 nodes_
    void registerNodeUnlocked(Node& node);

    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::string, Node*> nodes_;
    std::unordered_map<std::string, std::vector<Publisher*>> publishers_;
    std::unordered_map<std::string, std::vector<Subscriber*>> subscribers_;
    std::unordered_map<std::string, ServiceServer*> services_;
    bool shutdown_ = false;
};

}  // namespace ros_sim
