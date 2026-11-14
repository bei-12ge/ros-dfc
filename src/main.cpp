// 演示程序入口（阶段三）：验证分级日志与异常体系。
#include "ros/exception.h"
#include "ros/logger.h"

#include <iostream>

using namespace ros_sim;

int main() {
    Logger::instance().setLevel(LogLevel::INFO);
    Logger::instance().info("MAIN", "分级日志模块就绪");

    try {
        throw NodeException::emptyName();
    } catch (const RosException& ex) {
        Logger::instance().logException(ex);
    }

    std::cout << "ros_sim: 日志与异常体系构建成功" << std::endl;
    return 0;
}
