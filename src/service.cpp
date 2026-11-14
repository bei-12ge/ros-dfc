#include "ros/service.h"

#include <chrono>

#include "ros/exception.h"
#include "ros/logger.h"
#include "ros/ros.h"
#include "ros/util.h"

namespace ros_sim {

// ------------------------------------------------------------ ServiceServer
ServiceServer::ServiceServer(std::string name, std::string serviceName, ServiceHandler handler)
    : Node(std::move(name)), serviceName_(trim(serviceName)), handler_(std::move(handler)) {
    if (isBlank(serviceName_)) {
        throw ServiceException::emptyName();
    }
    if (!handler_) {
        throw ServiceException::emptyHandler(serviceName_);
    }
}

void ServiceServer::init() {
    if (isRunning()) {
        Logger::instance().warn("SERVICE", "服务端已初始化，忽略重复 init: " + name());
        return;
    }
    ROS::instance().registerService(*this);
    markRunning(true);
    Logger::instance().info("SERVICE", "服务端节点初始化完成: 节点=" + name() +
                                          ", 服务=" + serviceName_);
}

void ServiceServer::shutdown() {
    if (!isRunning()) {
        return;
    }
    ROS::instance().unregisterNode(name());
    markRunning(false);
    Logger::instance().info("SERVICE", "服务端节点已关闭: 节点=" + name() +
                                          ", 服务=" + serviceName_);
}

std::string ServiceServer::handle(const std::string& request) const {
    Logger::instance().info("SERVICE", "收到服务请求: 服务=" + serviceName_ + ", 请求=" + request);
    std::string response;
    try {
        response = handler_(request);
    } catch (const std::exception& ex) {
        // 处理函数内部异常不得穿透到调用方进程，统一包装为服务异常
        throw ServiceException::handlerFailed(serviceName_, ex.what());
    } catch (...) {
        throw ServiceException::handlerFailed(serviceName_, "未知异常");
    }
    handled_.fetch_add(1);
    Logger::instance().info("SERVICE", "服务响应完成: 服务=" + serviceName_ + ", 响应=" + response);
    return response;
}

// ------------------------------------------------------------ ServiceClient
ServiceClient::ServiceClient(std::string name, std::string serviceName)
    : Node(std::move(name)), serviceName_(trim(serviceName)) {
    if (isBlank(serviceName_)) {
        throw ServiceException::emptyName();
    }
}

void ServiceClient::init() {
    if (isRunning()) {
        Logger::instance().warn("SERVICE", "客户端已初始化，忽略重复 init: " + name());
        return;
    }
    markRunning(true);  // 客户端不占用注册表，但同样受生命周期约束
    Logger::instance().info("SERVICE", "客户端节点初始化完成: 节点=" + name() +
                                          ", 服务=" + serviceName_);
}

void ServiceClient::shutdown() {
    if (!isRunning()) {
        return;
    }
    markRunning(false);
    Logger::instance().info("SERVICE", "客户端节点已关闭: 节点=" + name() +
                                          ", 服务=" + serviceName_);
}

std::string ServiceClient::call(const std::string& request) {
    if (!isRunning()) {
        throw ServiceException::clientNotRunning(name());
    }
    const auto begin = std::chrono::steady_clock::now();
    const std::string response = ROS::instance().callService(serviceName_, request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    calls_.fetch_add(1);
    Logger::instance().info("SERVICE", "客户端调用完成: 节点=" + name() + ", 服务=" + serviceName_ +
                                          ", 请求=" + request + ", 响应=" + response +
                                          ", 耗时=" + std::to_string(elapsed) + "us");
    return response;
}

}  // namespace ros_sim
