# ros_sim —— C++ 模拟 ROS 节点系统

> 课程大作业：不依赖真实 ROS API，用 C++17 复刻 ROS 最核心的"节点—话题"发布/订阅通信模型。
> 覆盖节点抽象、消息封装、单例核心路由、异常体系与分级日志，并实现传感器自定义消息、服务/客户端同步通信、运行时动态增删节点等加分功能。

## 快速开始

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ros_demo        # 完整演示（7 幕）
cd build && ./ros_tests # 结构化测试（48 用例 / 114 断言）
```

或直接使用脚本：`./scripts/build_and_run.sh`、`./scripts/run_tests.sh`。

## 验证结果

| 项 | 结果 |
| --- | --- |
| 编译 | g++ 11.4 + CMake，`-Wall -Wextra -Wpedantic` **零警告** |
| 测试 | **48 个用例 / 114 个断言全部通过** |
| 内存检查 | ASan + UBSan 下测试与演示**零报告** |
| 性能 | 1 万条发布：1 订阅者 15.3 ms、3 订阅者 24.0 ms |

## 工程结构

```
ros_sim/
├── CMakeLists.txt          # 静态库 rossim + ros_demo + ros_tests
├── include/ros/            # 公开头文件（聚合头 ros_sim.h）
│   ├── exception.h         # 异常体系与错误码（NODE/TOPIC/MSG/SERVICE/ROS）
│   ├── logger.h            # 分级日志，控制台 + 文件
│   ├── message.h           # Message / SensorMessage
│   ├── node.h              # 节点抽象基类
│   ├── publisher.h / subscriber.h
│   ├── ros.h               # 单例核心：映射、路由、关闭、动态删除
│   ├── service.h           # 同步请求-响应
│   ├── timestamp.h / util.h
│   └── ros_sim.h
├── src/                    # 实现 + main.cpp（演示）
├── tests/                  # 轻量测试框架 + 4 组用例
├── docs/                   # 01~06 文档
└── scripts/                # 一键构建/测试脚本
```

## 核心设计要点

1. **`Node` 抽象 + 纯虚 `init/shutdown`**：核心只依赖抽象，`shutdownAll()` 统一关闭所有节点。
2. **`Message` 不可变**：构造即打时间戳，只有 const getter；转运用 `shared_ptr<const Message>`，O(1) 共享且防篡改。
3. **单例核心（Meyers Singleton）**：函数内静态变量，线程安全、延迟初始化、无初始化顺序问题。
4. **锁只保护映射表，回调一律锁外执行**：先拷订阅者副本再分发，避免持锁回调导致的死锁与迭代器失效。
5. **异常三要素（模块 / 错误码 / 级别）**：继承 `std::runtime_error`，兼容标准捕获，同时可分级记日志、可按错误码断言。
6. **零依赖**：不引入 gtest/boost，测试框架自研，评审机器上开箱即跑。

## 文档

| 文档 | 说明 |
| --- | --- |
| `docs/01-需求文档.md` | 范围、术语、功能/非功能需求（FR/NFR 编号）、验收标准、追踪矩阵 |
| `docs/02-架构设计文档.md` | 四层架构、类设计、时序图、7 个关键设计决策、接口契约 |
| `docs/03-技术文档.md` | 代码组织、7 个关键技术点、错误码总表、实测数据、已知限制 |
| `docs/04-用户手册.md` | 构建运行、5 分钟写第一个节点、API 速查、错误码速查、10 条 FAQ |
| `docs/05-测试用例文档.md` | 用例设计方法、48 条用例表、执行结果、覆盖率、缺陷记录 |
| `docs/06-答辩准备.md` | 3 分钟讲解稿、5 看点演示脚本、15 条高频问答 |

## 增量开发轨迹

```bash
git log --oneline
```

按"文档 → 骨架 → 异常/日志 → 消息 → 节点/核心 → 自定义消息 → 服务 → 动态节点 → 测试 → 文档"的顺序提交，每次提交均可独立编译运行。
