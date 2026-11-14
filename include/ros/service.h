#pragma once
// ---------------------------------------------------------------------------
// 服务 / 客户端：同步请求-响应通信（加分项 FR-07.2）
//   ServiceServer：注册一个具名服务与处理函数，收到请求后同步返回响应；
//   ServiceClient：发起调用并阻塞等待响应，返回处理结果。
//   与话题的区别：话题是"发布即忘、一对多、异步"；服务是"一问一答、一对一、同步"。
//
//   请求/响应在 v1 中统一用字符串承载（如 "1,2" → "3"），
//   需要结构化参数时由业务层自行解析；这样做避免引入序列化依赖。
// ---------------------------------------------------------------------------
#include <atomic>
#include <functional>
#include <string>

#include "ros/node.h"

namespace ros_sim {

/// 服务处理函数：输入请求串，返回响应串；允许抛异常，由服务层包装为 ServiceException
using ServiceHandler = std::function<std::string(const std::string& request)>;

class ServiceServer : public Node {
public:
    /// 服务名称为空抛 SERVICE-004；处理函数为空抛 SERVICE-005
    ServiceServer(std::string name, std::string serviceName, ServiceHandler handler);

    const std::string& serviceName() const noexcept { return serviceName_; }

    void init() override;
    void shutdown() override;

    /// 由 ROS 核心调用：执行处理函数并返回响应
    std::string handle(const std::string& request) const;

    /// 累计处理的请求数
    size_t handledCount() const noexcept { return handled_.load(); }

private:
    std::string serviceName_;
    ServiceHandler handler_;
    mutable std::atomic<size_t> handled_{0};
};

class ServiceClient : public Node {
public:
    /// 服务名称为空抛 SERVICE-004
    ServiceClient(std::string name, std::string serviceName);

    const std::string& serviceName() const noexcept { return serviceName_; }

    void init() override;
    void shutdown() override;

    /// 同步调用：阻塞直到服务端返回；服务不存在抛 SERVICE-001
    std::string call(const std::string& request);

    /// 累计调用次数
    size_t callCount() const noexcept { return calls_.load(); }

private:
    std::string serviceName_;
    std::atomic<size_t> calls_{0};
};

}  // namespace ros_sim
