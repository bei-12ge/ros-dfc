cmake_minimum_required(VERSION 3.16)

project(ros_sim
        VERSION 1.0.0
        DESCRIPTION "C++ 模拟 ROS 节点系统（发布/订阅模型）"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 严格编译选项：任何警告都视为需要修复的问题
add_compile_options(-Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------- 演示程序
add_executable(ros_demo src/main.cpp)
target_include_directories(ros_demo PRIVATE include)
