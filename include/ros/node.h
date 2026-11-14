#pragma once
// ---------------------------------------------------------------------------
// 节点抽象基类 Node
//   * 封装唯一的节点名称，只提供 const 读取接口；
//   * 定义初始化 / 关闭两个纯虚接口，强制子类实现"日志 + 注册/注销"；
//   * 统一节点生命周期状态（running_），并保证 shutdown 幂等；
//   * 析构时兜底注销，防止调用方漏掉 shutdown 导致核心持有悬空指针。
// ---------------------------------------------------------------------------
#include <string>

namespace ros_sim {

class Node {
public:
    /// 空/全空白节点名抛 NodeException(NODE-001)
    explicit Node(std::string name);
    virtual ~Node();

    // 节点是"有身份、有生命周期"的对象，禁止拷贝与赋值
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    /// 节点名称（只读）
    const std::string& name() const noexcept { return name_; }
    /// 是否处于已初始化、未关闭状态
    bool isRunning() const noexcept { return running_; }

    /// 初始化：子类实现"输出初始化日志 + 向 ROS 核心注册自身"
    virtual void init() = 0;
    /// 关闭：子类实现"输出关闭日志 + 从 ROS 核心注销"
    virtual void shutdown() = 0;

protected:
    /// 供子类在 init/shutdown 中切换生命周期状态
    void markRunning(bool running) noexcept { running_ = running; }

private:
    const std::string name_;
    bool running_ = false;
};

}  // namespace ros_sim
