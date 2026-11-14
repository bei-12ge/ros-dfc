#include "ros/node.h"

#include "ros/exception.h"
#include "ros/ros.h"
#include "ros/util.h"

namespace ros_sim {

Node::Node(std::string name)
    : name_(trim(name)) {
    if (isBlank(name_)) {
        throw NodeException::emptyName();
    }
}

Node::~Node() {
    // 兜底注销：调用方忘记 shutdown 时，避免核心残留悬空指针。
    // 析构中不允许异常逃逸，因此全部吞掉。
    if (!running_) {
        return;
    }
    try {
        ROS::instance().unregisterNode(name_);
    } catch (...) {
        // 忽略：进程退出阶段核心可能已析构
    }
}

}  // namespace ros_sim
