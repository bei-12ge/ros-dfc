#include "ros/message.h"

#include <iomanip>
#include <sstream>

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
    return typeName() + "{内容='" + content() + "', 时间戳='" + timestamp() + "'}";
}

// ------------------------------------------------------------- SensorMessage
SensorMessage::SensorMessage(const std::string& content, double temperature, double humidity)
    : Message(content), temperature_(temperature), humidity_(humidity) {
    // 物理量程校验：温度 [-40, 125]℃（常见工业传感器范围），湿度 [0, 100]%
    if (temperature < -40.0 || temperature > 125.0) {
        throw MessageException::invalidSensorValue("temperature", temperature);
    }
    if (humidity < 0.0 || humidity > 100.0) {
        throw MessageException::invalidSensorValue("humidity", humidity);
    }
}

std::string SensorMessage::typeName() const {
    return "sensor_msgs/SensorData";
}

std::string SensorMessage::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << typeName() << "{温度=" << temperature_ << "℃, 湿度=" << humidity_ << "%, 内容='"
        << content() << "', 时间戳='" << timestamp() << "'}";
    return oss.str();
}

}  // namespace ros_sim
