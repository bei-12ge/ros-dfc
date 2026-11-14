// 测试运行器：执行全部注册用例，输出分组结果与汇总，失败时返回退出码 1
#include "test_framework.h"

#include "ros/logger.h"

#include <iostream>
#include <string>

int main() {
    // 用例执行期间关闭业务日志的控制台输出，保持测试结果干净
    ros_sim::Logger::instance().enableConsole(false);
    ros_sim::Logger::instance().setLevel(ros_sim::LogLevel::WARN);

    std::cout << "============================================================\n"
              << "  ros_sim 结构化测试用例执行\n"
              << "============================================================\n";

    int caseCount = 0;
    int failedCases = 0;
    std::string currentSuite;

    for (const auto& testCase : ::testing::registry()) {
        if (testCase.suite != currentSuite) {
            currentSuite = testCase.suite;
            std::cout << "\n[" << currentSuite << "]\n";
        }
        const int failuresBefore = ::testing::stats().failures;
        try {
            testCase.body();
        } catch (const std::exception& ex) {
            ::testing::reportFailure(__FILE__, __LINE__,
                                     std::string("用例抛出未捕获异常: ") + ex.what());
        } catch (...) {
            ::testing::reportFailure(__FILE__, __LINE__, "用例抛出未知异常");
        }
        ++caseCount;
        const bool passed = ::testing::stats().failures == failuresBefore;
        if (!passed) {
            ++failedCases;
        }
        std::cout << (passed ? "  [通过] " : "  [失败] ") << testCase.name << "\n";
    }

    std::cout << "\n------------------------------------------------------------\n"
              << "  用例总数: " << caseCount << "，通过: " << (caseCount - failedCases)
              << "，失败: " << failedCases << "\n"
              << "  断言总数: " << ::testing::stats().checks
              << "，失败断言: " << ::testing::stats().failures << "\n"
              << "------------------------------------------------------------\n";

    if (failedCases == 0) {
        std::cout << "  测试结果: 全部通过 ✅\n";
        return 0;
    }
    std::cout << "  测试结果: 存在失败 ❌\n";
    return 1;
}
