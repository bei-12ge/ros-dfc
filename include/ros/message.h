#pragma once
// ---------------------------------------------------------------------------
// 消息封装类 Message
//   * 只封装"字符串内容 + 创建时间戳"，构造时自动打时间戳；
//   * 只提供 const 读取接口，无 setter，保证数据不可变与封装性；
//   * timestamp() 固定格式 "YYYY-MM-DD HH:MM:SS"；
//   * 通过虚函数 typeName()/toString() 支持"自定义消息类型"扩展（加分项 FR-07.1）。
// ---------------------------------------------------------------------------
#include <string>

namespace ros_sim {

class Message {
public:
    /// 构造即打时间戳；内容为空/全空白时抛 MessageException(MSG-001)
    explicit Message(const std::string& content);

    virtual ~Message() = default;

    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    /// 业务内容（只读）
    const std::string& content() const noexcept { return content_; }
    /// 创建时间戳，格式 "YYYY-MM-DD HH:MM:SS"（只读）
    const std::string& timestamp() const noexcept { return timestamp_; }

    /// 消息类型名，供路由/日志区分不同消息。默认 std_msgs/String
    virtual std::string typeName() const;
    /// 统一的可读文本，供日志输出
    virtual std::string toString() const;

protected:
    /// 供子类指定时间戳（如反序列化历史消息）使用
    Message(std::string content, std::string timestamp);

private:
    std::string content_;
    std::string timestamp_;
};

// ---------------------------------------------------------------------------
// 自定义消息类型：传感器数据（加分项 FR-07.1）
//   * 在字符串内容之外扩展温度、湿度两个业务字段；
//   * 通过覆盖 typeName()/toString() 参与路由与日志，核心转发层无需任何改动。
// ---------------------------------------------------------------------------
class SensorMessage : public Message {
public:
    SensorMessage(const std::string& content, double temperature, double humidity);

    double temperature() const noexcept { return temperature_; }
    double humidity() const noexcept { return humidity_; }

    std::string typeName() const override;
    std::string toString() const override;

private:
    double temperature_;
    double humidity_;
};

}  // namespace ros_sim
