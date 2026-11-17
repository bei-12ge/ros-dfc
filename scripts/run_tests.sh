#!/usr/bin/env bash
# 一键构建并执行结构化测试（失败即退出码非 0）
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cd build && ./ros_tests
