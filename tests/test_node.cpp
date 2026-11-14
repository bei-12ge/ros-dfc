// 节点模块测试：Node / Publisher / Subscriber
// 覆盖 TC-101 ~ TC-104、TC-201 ~ TC-207、TC-301 ~ TC-305
#include "ros/logger.h"
#include "ros/message.h"
#include "ros/node.h"
#include "ros/publisher.h"
#include "ros/ros.h"
#include "ros/subscriber.h"

#include "test_framework.h"

using namespace ros_sim;

namespace {

/// 每个用例独立的核心状态（避免用例间的注册表污染）
struct CoreFixture {
    CoreFixture() { ROS::instance().reset(); }
    ~CoreFixture() { ROS::instance().reset(); }
};

}  // namespace

// ------------------------------------------------------------------ Node
TEST_CASE(node, 正常构造后名称为只读且未运行) {
    CoreFixture core;
    Publisher publisher("pub_1", "topic/a");
    CHECK_STR_EQ(publisher.name(), "pub_1");
    CHECK_STR_EQ(publisher.topic(), "topic/a");
    CHECK(!publisher.isRunning());
}

TEST_CASE(node, 空节点名抛NODE_001) {
    CoreFixture core;
    CHECK_THROWS_CODE("NODE-001", Publisher bad("", "topic/a"));
    CHECK_THROWS_CODE("NODE-001", Subscriber other("   ", "topic/a"));
}

TEST_CASE(node, 纯空白节点名同样被拒绝) {
    CoreFixture core;
    CHECK_THROWS_CODE("NODE-001", Subscriber blank("\t\r\n ", "topic/a"));
}

TEST_CASE(node, 名称首尾空白被裁剪) {
    CoreFixture core;
    Publisher publisher("  pub_trim  ", "topic/a");
    CHECK_STR_EQ(publisher.name(), "pub_trim");
}

// ------------------------------------------------------------------ Publisher
TEST_CASE(publisher, 发布消息送达全部订阅者并计数) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subA("sub_a", "sensor/temp");
    Subscriber subB("sub_b", "sensor/temp");
    publisher.init();
    subA.init();
    subB.init();

    const size_t delivered = publisher.publish("24.5");
    CHECK_EQ(delivered, 2);
    CHECK_EQ(publisher.publishedCount(), 1);
    CHECK_EQ(subA.receivedCount(), 1);
    CHECK_EQ(subB.receivedCount(), 1);
}

TEST_CASE(publisher, 初始化后注册表反映映射关系) {
    CoreFixture core;
    Publisher publisher("pub_t", "robot/status");
    publisher.init();
    CHECK_EQ(ROS::instance().publisherCount("robot/status"), 1);
    CHECK_EQ(ROS::instance().publishersOf("robot/status").size(), 1);
    CHECK_STR_EQ(ROS::instance().publishersOf("robot/status")[0], "pub_t");
}

TEST_CASE(publisher, 空消息内容发布抛MSG_001) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    publisher.init();
    CHECK_THROWS_CODE("MSG-001", publisher.publish(""));
    CHECK_THROWS_CODE("MSG-001", publisher.publish("   "));
}

TEST_CASE(publisher, 空话题名抛TOPIC_001) {
    CoreFixture core;
    CHECK_THROWS_CODE("TOPIC-001", Publisher bad("pub_bad", ""));
    CHECK_THROWS_CODE("TOPIC-001", Publisher bad2("pub_bad2", "  \t"));
}

TEST_CASE(publisher, 未初始化即发布抛NODE_003) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    CHECK_THROWS_CODE("NODE-003", publisher.publish("24.5"));
}

TEST_CASE(publisher, 关闭幂等且注销后计数归零) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    publisher.init();
    publisher.shutdown();
    CHECK(!publisher.isRunning());
    CHECK_EQ(ROS::instance().publisherCount("sensor/temp"), 0);
    CHECK_NO_THROW(publisher.shutdown());  // 第二次关闭不抛异常
}

TEST_CASE(publisher, 可发布已有Message对象且内容保持不变) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();

    const Message message("预构造消息");
    CHECK_EQ(publisher.publish(message), 1);
    CHECK_EQ(subscriber.receivedCount(), 1);
}

// ------------------------------------------------------------------ Subscriber
TEST_CASE(subscriber, 接收消息后计数递增) {
    CoreFixture core;
    Subscriber subscriber("sub_a", "sensor/temp");
    subscriber.init();
    const auto message = std::make_shared<const Message>("30.1");
    subscriber.receive(message);
    subscriber.receive(message);
    CHECK_EQ(subscriber.receivedCount(), 2);
}

TEST_CASE(subscriber, 业务回调可读取消息内容与时间戳) {
    CoreFixture core;
    std::string capturedContent;
    std::string capturedTimestamp;
    Subscriber subscriber("sub_a", "sensor/temp",
                          [&](const Subscriber& self, const std::shared_ptr<const Message>& msg) {
                              capturedContent = msg->content();
                              capturedTimestamp = msg->timestamp();
                              CHECK_STR_EQ(self.name(), "sub_a");
                          });
    subscriber.init();
    subscriber.receive(std::make_shared<const Message>("回调内容"));
    CHECK_STR_EQ(capturedContent, "回调内容");
    CHECK(capturedTimestamp.size() == 19);
}

TEST_CASE(subscriber, 回调抛出异常被隔离且不影响其他订阅者) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber bad("sub_bad", "sensor/temp", [](const Subscriber&, const std::shared_ptr<const Message>&) {
        throw std::runtime_error("业务回调故意失败");
    });
    Subscriber good("sub_good", "sensor/temp");
    publisher.init();
    bad.init();
    good.init();

    CHECK_NO_THROW(publisher.publish("payload"));
    CHECK_EQ(good.receivedCount(), 1);  // 坏回调不影响好订阅者
}

TEST_CASE(subscriber, 关闭后不再收到消息) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();
    subscriber.shutdown();
    CHECK_EQ(publisher.publish("after shutdown"), 0);
    CHECK_EQ(subscriber.receivedCount(), 0);
}

TEST_CASE(subscriber, 接收空消息对象抛MSG_002) {
    CoreFixture core;
    Subscriber subscriber("sub_a", "sensor/temp");
    subscriber.init();
    const std::shared_ptr<const Message> nullMessage;
    CHECK_THROWS_CODE("MSG-002", subscriber.receive(nullMessage));
}
