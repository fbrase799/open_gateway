#pragma once

#include <functional>
#include <memory>
#include <string>

namespace ogw {

class MqttClient {
public:
    using CommandHandler =
        std::function<void(const std::string& device_id, const std::string& capability_id, const std::string& payload)>;

    explicit MqttClient(std::string url);
    ~MqttClient();

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    void set_command_handler(CommandHandler handler);
    void start();
    void stop();
    void publish(std::string topic, std::string payload, bool retained);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ogw
