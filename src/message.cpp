#include "ros/message.h"

#include "ros/exception.h"
#include "ros/timestamp.h"
#include "ros/util.h"

namespace ros_sim {

Message::Message(const std::string& content)
    : content_(content), timestamp_(nowTimestamp()) {
    if (isBlank(content_)) {
        throw MessageException::emptyContent();
    }
}

Message::Message(std::string content, std::string timestamp)
    : content_(std::move(content)), timestamp_(std::move(timestamp)) {
    if (isBlank(content_)) {
        throw MessageException::emptyContent();
    }
    if (timestamp_.empty()) {
        timestamp_ = nowTimestamp();
    }
}

std::string Message::typeName() const {
    return "std_msgs/String";
}

std::string Message::toString() const {
    return typeName() + "{内容='" + content_ + "', 时间戳='" + timestamp_ + "'}";
}

}  // namespace ros_sim
