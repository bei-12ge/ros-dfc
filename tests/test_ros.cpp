// ROS 核心模块测试：单例、映射、路由、全局关闭、动态删除（TC-501 ~ TC-513、TC-601）
#include "ros/message.h"
#include "ros/publisher.h"
#include "ros/ros.h"
#include "ros/subscriber.h"

#include <string>
#include <thread>
#include <vector>

#include "test_framework.h"

using namespace ros_sim;

namespace {

struct CoreFixture {
    CoreFixture() { ROS::instance().reset(); }
    ~CoreFixture() { ROS::instance().reset(); }
};

}  // namespace

TEST_CASE(ros, 单例全局唯一且地址相同) {
    CoreFixture core;
    CHECK(&ROS::instance() == &ROS::instance());
}

TEST_CASE(ros, 注册后话题与节点映射同步更新) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();

    CHECK_EQ(ROS::instance().nodeNames().size(), 2);
    CHECK_EQ(ROS::instance().publisherCount("sensor/temp"), 1);
    CHECK_EQ(ROS::instance().subscriberCount("sensor/temp"), 1);
    CHECK_EQ(ROS::instance().topics().size(), 1);
    CHECK(ROS::instance().findNode("pub_t") != nullptr);
}

TEST_CASE(ros, 多发布者同话题时订阅者全部收到) {
    CoreFixture core;
    Publisher pubA("pub_a", "sensor/temp");
    Publisher pubB("pub_b", "sensor/temp");
    Subscriber subscriber("sub_x", "sensor/temp");
    pubA.init();
    pubB.init();
    subscriber.init();

    pubA.publish("from_a");
    pubB.publish("from_b");
    CHECK_EQ(subscriber.receivedCount(), 2);
    CHECK_EQ(ROS::instance().publisherCount("sensor/temp"), 2);
}

TEST_CASE(ros, 重复节点名注册抛NODE_002) {
    CoreFixture core;
    Publisher publisher("same_name", "topic/a");
    publisher.init();
    // 跨类型同名也应被拒绝
    Subscriber subscriber("same_name", "topic/b");
    CHECK_THROWS_CODE("NODE-002", subscriber.init());
}

TEST_CASE(ros, 向不存在话题转发抛TOPIC_002) {
    CoreFixture core;
    Publisher publisher("pub_t", "topic/exist");
    publisher.init();
    const auto message = std::make_shared<const Message>("孤立消息");
    CHECK_THROWS_CODE("TOPIC-002", ROS::instance().publish("topic/missing", message));
}

TEST_CASE(ros, 转发空话题名抛TOPIC_001_空消息抛MSG_002) {
    CoreFixture core;
    Publisher publisher("pub_t", "topic/exist");
    publisher.init();
    const auto message = std::make_shared<const Message>("x");
    CHECK_THROWS_CODE("TOPIC-001", ROS::instance().publish("  ", message));
    const std::shared_ptr<const Message> nullMessage;
    CHECK_THROWS_CODE("MSG-002", ROS::instance().publish("topic/exist", nullMessage));
}

TEST_CASE(ros, 话题无订阅者时返回0且不崩溃) {
    CoreFixture core;
    Publisher publisher("pub_t", "topic/lonely");
    publisher.init();
    const auto message = std::make_shared<const Message>("无人接收");
    CHECK_EQ(publisher.publish(message), 0);
    CHECK_EQ(publisher.publishedCount(), 1);
}

TEST_CASE(ros, 全局关闭所有节点且幂等) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();

    ROS::instance().shutdownAll();
    CHECK(!publisher.isRunning());
    CHECK(!subscriber.isRunning());
    CHECK(ROS::instance().isShutdown());
    CHECK_EQ(ROS::instance().nodeNames().size(), 0);

    CHECK_NO_THROW(ROS::instance().shutdownAll());  // 幂等
}

TEST_CASE(ros, 关闭后拒绝新节点注册抛ROS_001) {
    CoreFixture core;
    ROS::instance().shutdownAll();
    Publisher publisher("pub_late", "sensor/temp");
    CHECK_THROWS_CODE("ROS-001", publisher.init());
}

TEST_CASE(ros, 运行时删除节点后不再收到消息) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();

    ROS::instance().removeNode("sub_a");
    CHECK(!subscriber.isRunning());
    CHECK_EQ(ROS::instance().subscriberCount("sensor/temp"), 0);
    CHECK_EQ(publisher.publish("被丢弃"), 0);
    CHECK_EQ(subscriber.receivedCount(), 0);
}

TEST_CASE(ros, 删除不存在的节点抛NODE_003) {
    CoreFixture core;
    CHECK_THROWS_CODE("NODE-003", ROS::instance().removeNode("ghost"));
}

TEST_CASE(ros, 注销后映射被清理干净) {
    CoreFixture core;
    Publisher publisher("pub_t", "sensor/temp");
    Subscriber subscriber("sub_a", "sensor/temp");
    publisher.init();
    subscriber.init();
    subscriber.shutdown();
    publisher.shutdown();

    CHECK_EQ(ROS::instance().nodeNames().size(), 0);
    CHECK_EQ(ROS::instance().topics().size(), 0);
    CHECK_EQ(ROS::instance().subscriberCount("sensor/temp"), 0);
}

TEST_CASE(ros, 全生命周期集成_初始化_收发_关闭) {
    CoreFixture core;
    Publisher publisher("pub_temp", "sensor/temperature");
    Subscriber monitorA("sub_a", "sensor/temperature");
    Subscriber monitorB("sub_b", "sensor/temperature");
    publisher.init();
    monitorA.init();
    monitorB.init();

    for (int i = 0; i < 10; ++i) {
        publisher.publish(std::to_string(i) + ".0");
    }
    CHECK_EQ(publisher.publishedCount(), 10);
    CHECK_EQ(monitorA.receivedCount(), 10);
    CHECK_EQ(monitorB.receivedCount(), 10);

    ROS::instance().shutdownAll();
    CHECK_EQ(ROS::instance().nodeNames().size(), 0);
}

TEST_CASE(ros, 四线程并发发布两订阅者计数一致) {
    CoreFixture core;
    constexpr int kThreads = 4;
    constexpr int kPerThread = 500;

    Publisher publisher("pub_mt", "sensor/mt");
    Subscriber subscriberA("sub_mt_a", "sensor/mt");
    Subscriber subscriberB("sub_mt_b", "sensor/mt");
    publisher.init();
    subscriberA.init();
    subscriberB.init();

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&publisher, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                publisher.publish(std::to_string(t) + "-" + std::to_string(i));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    CHECK_EQ(publisher.publishedCount(), kThreads * kPerThread);
    CHECK_EQ(subscriberA.receivedCount(), kThreads * kPerThread);
    CHECK_EQ(subscriberB.receivedCount(), kThreads * kPerThread);
}
