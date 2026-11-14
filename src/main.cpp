// ---------------------------------------------------------------------------
// ros_demo：完整演示程序
//   1) 初始化多发布者 / 多订阅者拓扑并输出初始化日志
//   2) 连续发布消息，观察异步收发
//   3) 演示各类非法操作被异常拦截（进程不崩溃）
//   4) 全局关闭，日志落盘
// ---------------------------------------------------------------------------
#include "ros/ros_sim.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

using namespace ros_sim;

namespace {

/// 演示用回调：证明订阅者可以把消息交给业务逻辑
Subscriber::Callback makeBusinessCallback(const std::string& tag) {
    return [tag](const Subscriber&, const std::shared_ptr<const Message>& message) {
        Logger::instance().info("CALLBACK", tag + " 业务回调收到: " + message->content());
    };
}

/// 执行一段"应当抛异常"的操作，并打印拦截结果
template <typename Action>
void expectException(const std::string& label, Action&& action) {
    try {
        action();
        std::cout << "  [不符合预期] " << label << " 未抛出异常\n";
    } catch (const RosException& ex) {
        std::cout << "  [已拦截] " << label << " → " << ex.code() << " / " << ex.what() << "\n";
        Logger::instance().logException(ex);
    }
}

void printBanner() {
    std::cout << "\n============================================================\n"
              << "  ros_sim —— C++ 模拟 ROS 节点系统演示\n"
              << "  多发布者 / 多订阅者 / 话题路由 / 异常拦截 / 日志落盘\n"
              << "============================================================\n\n";
}

}  // namespace

int main() {
    Logger& logger = Logger::instance();
    logger.setLevel(LogLevel::INFO);
    std::filesystem::create_directories("logs");
    logger.addFileSink("logs/ros_sim.log");

    printBanner();

    // ---------------------------------------------------------- 1. 构建拓扑
    std::cout << "--- 1. 创建节点并初始化 ---\n";
    auto tempPublisher = std::make_unique<Publisher>("pub_temp", "sensor/temperature");
    auto statusPublisher = std::make_unique<Publisher>("pub_status", "robot/status");

    auto monitorA = std::make_unique<Subscriber>("sub_monitor_a", "sensor/temperature",
                                                 makeBusinessCallback("监视器A"));
    auto monitorB = std::make_unique<Subscriber>("sub_monitor_b", "sensor/temperature");
    auto dashBoard = std::make_unique<Subscriber>("sub_dashboard", "robot/status",
                                                  makeBusinessCallback("看板"));

    tempPublisher->init();
    statusPublisher->init();
    monitorA->init();
    monitorB->init();
    dashBoard->init();

    std::cout << "  当前节点: ";
    for (const auto& name : ROS::instance().nodeNames()) {
        std::cout << name << "  ";
    }
    std::cout << "\n\n";

    // ---------------------------------------------------------- 2. 发布消息
    std::cout << "--- 2. 发布者连续发布（无需等待订阅者应答）---\n";
    for (int i = 1; i <= 10; ++i) {
        const std::string payload = std::to_string(20 + i) + ".5";
        const size_t delivered = tempPublisher->publish(payload);
        std::cout << "  pub_temp 发布 " << payload << "℃，送达订阅者 " << delivered << " 个\n";
    }
    statusPublisher->publish("battery=87%, mode=auto");
    statusPublisher->publish("battery=85%, mode=auto");
    std::cout << "\n";

    // ---------------------------------------------------------- 3. 异常演示
    std::cout << "--- 3. 非法操作拦截（进程必须存活）---\n";
    expectException("空节点名", [] { Publisher bad("   ", "topic"); });
    expectException("重复节点名注册", [&] { Subscriber duplicate("pub_temp", "sensor/temperature"); duplicate.init(); });
    expectException("空消息内容发布", [&] { tempPublisher->publish(""); });
    expectException("向不存在的话题转发", [&] {
        Message message("孤立消息");
        ROS::instance().publish("topic/not_exist", std::make_shared<const Message>(message));
    });
    expectException("发布空消息对象", [&] {
        const std::shared_ptr<const Message> nullMessage;
        tempPublisher->publish(nullMessage);
    });
    expectException("空话题名", [] { Subscriber bad("sub_bad", " "); });
    std::cout << "  以上异常均被捕获，演示继续执行\n\n";

    // ---------------------------------------------------------- 4. 收发统计
    std::cout << "--- 4. 运行统计 ---\n";
    std::cout << "  pub_temp 累计发布: " << tempPublisher->publishedCount() << " 条\n";
    std::cout << "  sub_monitor_a 累计接收: " << monitorA->receivedCount() << " 条\n";
    std::cout << "  sub_monitor_b 累计接收: " << monitorB->receivedCount() << " 条\n";
    std::cout << "  sub_dashboard 累计接收: " << dashBoard->receivedCount() << " 条\n";
    std::cout << "  话题列表: ";
    for (const auto& topic : ROS::instance().topics()) {
        std::cout << topic << "  ";
    }
    std::cout << "\n\n";

    // ---------------------------------------------------------- 5. 全局关闭
    std::cout << "--- 5. 全局关闭 ---\n";
    ROS::instance().shutdownAll();

    expectException("对已关闭节点发布", [&] { tempPublisher->publish("after shutdown"); });

    std::cout << "\n演示结束。详细日志见 logs/ros_sim.log\n";
    logger.closeFileSink();
    return 0;
}
