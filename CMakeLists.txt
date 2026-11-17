cmake_minimum_required(VERSION 3.16)

project(ros_sim
        VERSION 1.0.0
        DESCRIPTION "C++ 模拟 ROS 节点系统（发布/订阅模型）"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 严格编译选项：任何警告都视为需要修复的问题
# MSVC（Visual Studio / CLion 的 Visual Studio 工具链）不认识 -Wall，需按编译器分支
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------- 核心静态库
add_library(rossim STATIC
    src/exception.cpp
    src/logger.cpp
    src/message.cpp
    src/node.cpp
    src/publisher.cpp
    src/subscriber.cpp
    src/ros.cpp
    src/service.cpp
    src/report.cpp
)
target_include_directories(rossim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------- 演示程序
add_executable(ros_demo src/main.cpp)
target_link_libraries(ros_demo PRIVATE rossim)

# ---------------------------------------------------------------- 测试
enable_testing()
add_executable(ros_tests
    tests/test_main.cpp
    tests/test_message.cpp
    tests/test_node.cpp
    tests/test_ros.cpp
    tests/test_bonus.cpp
)
target_link_libraries(ros_tests PRIVATE rossim)
target_include_directories(ros_tests PRIVATE tests)
add_test(NAME ros_tests COMMAND ros_tests)
# 测试运行时的日志/临时文件统一放在构建目录下的 logs/
set_tests_properties(ros_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
