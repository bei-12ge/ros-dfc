// 加分功能与基础设施测试：传感器消息、服务通信、异常体系、日志落盘
// 覆盖 TC-407、TC-701 ~ TC-710、TC-801 ~ TC-803
#include "ros/exception.h"
#include "ros/logger.h"
#include "ros/message.h"
#include "ros/publisher.h"
#include "ros/ros.h"
#include "ros/service.h"
#include "ros/subscriber.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "test_framework.h"

using namespace ros_sim;

namespace {

struct CoreFixture {
    CoreFixture() { ROS::instance().reset(); }
    ~CoreFixture() { ROS::instance().reset(); }
};

std::string readAll(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

// ------------------------------------------------------------ 自定义消息
TEST_CASE(bonus_sensor, 传感器消息经共享指针转发后保留多态类型) {
    CoreFixture core;
    double capturedTemperature = 0.0;
    std::string capturedType;
    Subscriber subscriber("sub_sensor", "sensor/th",
                          [&](const Subscriber&, const std::shared_ptr<const Message>& msg) {
                              capturedType = msg->typeName();
                              const auto* sensor = dynamic_cast<const SensorMessage*>(msg.get());
                              CHECK(sensor != nullptr);
                              if (sensor != nullptr) {
                                  capturedTemperature = sensor->temperature();
                              }
                          });
    Publisher publisher("pub_sensor", "sensor/th");
    publisher.init();
    subscriber.init();

    publisher.publish(std::make_shared<const SensorMessage>("采样", 26.5, 61.2));
    CHECK_STR_EQ(capturedType, "sensor_msgs/SensorData");
    CHECK(capturedTemperature > 26.4 && capturedTemperature < 26.6);
}

// ------------------------------------------------------------ 服务通信
TEST_CASE(bonus_service, 同步调用返回处理结果) {
    CoreFixture core;
    ServiceServer server("srv_add", "service/add",
                         [](const std::string& request) { return "response:" + request; });
    ServiceClient client("cli_add", "service/add");
    server.init();
    client.init();

    const std::string response = client.call("ping");
    CHECK_STR_EQ(response, "response:ping");
    CHECK_EQ(client.callCount(), 1);
    CHECK_EQ(server.handledCount(), 1);
    CHECK(ROS::instance().findService("service/add") != nullptr);
}

TEST_CASE(bonus_service, 调用未注册服务抛SERVICE_001) {
    CoreFixture core;
    ServiceClient client("cli_ghost", "service/ghost");
    client.init();
    CHECK_THROWS_CODE("SERVICE-001", client.call("ping"));
}

TEST_CASE(bonus_service, 处理函数抛异常被包装为SERVICE_002) {
    CoreFixture core;
    ServiceServer server("srv_boom", "service/boom",
                         [](const std::string&) -> std::string {
                             throw std::runtime_error("服务内部故障");
                         });
    ServiceClient client("cli_boom", "service/boom");
    server.init();
    client.init();
    CHECK_THROWS_CODE("SERVICE-002", client.call("request"));
    CHECK_EQ(server.handledCount(), 0);  // 失败请求不计入成功处理数
}

TEST_CASE(bonus_service, 服务名重复注册抛SERVICE_006) {
    CoreFixture core;
    ServiceServer first("srv_1", "service/dup", [](const std::string&) { return std::string("1"); });
    ServiceServer second("srv_2", "service/dup", [](const std::string&) { return std::string("2"); });
    first.init();
    CHECK_THROWS_CODE("SERVICE-006", second.init());
}

TEST_CASE(bonus_service, 客户端未初始化即调用抛SERVICE_003) {
    CoreFixture core;
    ServiceClient client("cli_idle", "service/add");
    CHECK_THROWS_CODE("SERVICE-003", client.call("request"));
}

// ------------------------------------------------------------ 异常体系
TEST_CASE(exception, 自定义异常可被std_exception捕获) {
    CoreFixture core;
    bool caught = false;
    try {
        const Message empty("");
    } catch (const std::exception&) {
        caught = true;
    }
    CHECK(caught);

    try {
        ROS::instance().removeNode("ghost");
    } catch (const std::runtime_error&) {  // RosException 继承 runtime_error
        caught = true;
    } catch (...) {
        caught = false;
    }
    CHECK(caught);
}

TEST_CASE(exception, 异常日志串包含模块与错误码) {
    CoreFixture core;
    const std::string log = NodeException::emptyName().toLogString();
    CHECK(log.find("[NODE]") != std::string::npos);
    CHECK(log.find("[NODE-001]") != std::string::npos);
    CHECK(log.find("节点名称不能为空") != std::string::npos);
}

TEST_CASE(exception, 异常级别按严重程度分级) {
    CoreFixture core;
    CHECK(NodeException::emptyName().level() == LogLevel::ERROR);
    CHECK(NodeException::notFound("x").level() == LogLevel::WARN);
    CHECK(TopicException::notFound("x").level() == LogLevel::WARN);
    CHECK(MessageException::emptyContent().level() == LogLevel::ERROR);
}

// ------------------------------------------------------------ 日志
TEST_CASE(logger, 日志落盘并含时间戳级别模块) {
    std::filesystem::create_directories("logs");
    const std::string path = "logs/test_logger.log";
    std::filesystem::remove(path);

    Logger::instance().setLevel(LogLevel::INFO);
    CHECK(Logger::instance().addFileSink(path));
    Logger::instance().info("TEST", "落盘测试消息");
    Logger::instance().warn("TEST", "告警测试消息");
    Logger::instance().closeFileSink();
    CHECK(!Logger::instance().hasFileSink());

    const std::string content = readAll(path);
    CHECK(content.find("[INFO][TEST] 落盘测试消息") != std::string::npos);
    CHECK(content.find("[WARN][TEST] 告警测试消息") != std::string::npos);
    CHECK(content.find("落盘测试消息") != std::string::npos);
}

TEST_CASE(logger, 级别过滤生效) {
    std::filesystem::create_directories("logs");
    const std::string path = "logs/test_level.log";
    std::filesystem::remove(path);

    Logger::instance().addFileSink(path);
    Logger::instance().setLevel(LogLevel::ERROR);
    Logger::instance().info("TEST", "该条应被过滤");
    Logger::instance().error("TEST", "该条应保留");
    Logger::instance().closeFileSink();
    Logger::instance().setLevel(LogLevel::INFO);  // 恢复默认

    const std::string content = readAll(path);
    CHECK(content.find("该条应被过滤") == std::string::npos);
    CHECK(content.find("该条应保留") != std::string::npos);
}
