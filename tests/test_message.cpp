// 消息封装模块测试（TC-401 ~ TC-407）
#include "ros/message.h"
#include "ros/timestamp.h"

#include <regex>
#include <string>
#include <type_traits>

#include "test_framework.h"

using namespace ros_sim;

TEST_CASE(message, 时间戳格式为YYYY_MM_DD_HH_MM_SS) {
    const Message message("hello ros");
    const std::regex pattern(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
    CHECK(std::regex_match(message.timestamp(), pattern));
    CHECK_EQ(message.timestamp().size(), 19);
}

TEST_CASE(message, 时间戳为当前本地时间_误差不超过2秒) {
    const std::time_t before = std::time(nullptr);
    const Message message("tick");
    const std::time_t after = std::time(nullptr);
    // 允许 2 秒内的跨秒/调度抖动
    CHECK(message.timestamp() == formatTimestamp(before) ||
          message.timestamp() == formatTimestamp(before + 1) ||
          message.timestamp() == formatTimestamp(after) ||
          message.timestamp() == formatTimestamp(after - 1));
}

TEST_CASE(message, 内容与时间戳只读且无setter) {
    const Message message("只读");
    static_assert(std::is_same<decltype(message.content()), const std::string&>::value,
                  "content() 必须返回 const 引用");
    static_assert(std::is_same<decltype(message.timestamp()), const std::string&>::value,
                  "timestamp() 必须返回 const 引用");
    CHECK_STR_EQ(message.content(), "只读");
}

TEST_CASE(message, 空内容构造抛MSG_001) {
    CHECK_THROWS_CODE("MSG-001", Message empty(""));
    CHECK_THROWS_CODE("MSG-001", Message blank("   \t\n "));
}

TEST_CASE(message, 拷贝后内容与时间戳独立) {
    const Message origin("原始内容");
    Message copy = origin;
    CHECK_STR_EQ(copy.content(), origin.content());
    CHECK_STR_EQ(copy.timestamp(), origin.timestamp());
    // 拷贝对象与原对象是两个独立存储
    CHECK(copy.content().data() != origin.content().data());
}

TEST_CASE(message, SensorMessage携带温湿度且类型名不同) {
    const SensorMessage sensor("采样", 26.5, 61.2);
    CHECK_STR_EQ(sensor.typeName(), "sensor_msgs/SensorData");
    CHECK(sensor.temperature() > 26.4 && sensor.temperature() < 26.6);
    CHECK(sensor.humidity() > 61.1 && sensor.humidity() < 61.3);
    CHECK(sensor.toString().find("26.5") != std::string::npos);
    // 基类 typeName 应为 std_msgs/String
    const Message& base = sensor;
    CHECK_STR_EQ(base.typeName(), "sensor_msgs/SensorData");  // 多态：取到子类实现
}

TEST_CASE(message, 传感器数值边界校验抛MSG_003) {
    // 边界值合法
    CHECK_NO_THROW(SensorMessage ok1("边界", 125.0, 100.0));
    CHECK_NO_THROW(SensorMessage ok2("边界", -40.0, 0.0));
    // 越界非法
    CHECK_THROWS_CODE("MSG-003", SensorMessage bad1("越界", 125.1, 50.0));
    CHECK_THROWS_CODE("MSG-003", SensorMessage bad2("越界", 20.0, -1.0));
}
