#include "can_decode.hpp"
#include "ogw/sdk/plugin.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

namespace {

CanFrameMap showcase_frame() {
    CanFrameMap frame;
    frame.id = 0x101;
    frame.deviceLocalId = "showcase";
    frame.displayName = "Bare-metal showcase";
    frame.signals.push_back({"encoder_counts", "number", "counts", 0, 4, 1, true});
    frame.signals.push_back({"duty", "number", "%", 4, 1, 1, false});
    frame.signals.push_back({"direction", "number", "", 5, 1, 1, false});
    return frame;
}

class CanMotorPlugin final : public ogw::sdk::Plugin {
public:
    ogw::sdk::Info info() const override { return {"can-motor", "CAN motor", "0.1.0"}; }

    void init(std::string_view config) override {
        const auto marker = config.find("\"interface\"");
        if (marker != std::string_view::npos) {
            const auto colon = config.find(':', marker);
            const auto open = config.find('"', colon == std::string_view::npos ? marker : colon + 1);
            const auto close = open == std::string_view::npos ? std::string_view::npos : config.find('"', open + 1);
            if (open != std::string_view::npos && close != std::string_view::npos && close > open + 1) {
                interface_ = std::string(config.substr(open + 1, close - open - 1));
            }
        }
        frame_ = showcase_frame();
        stale_after_ms_ = 1000;
    }

    void start() override {
        socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (socket_ < 0) {
            throw ogw::sdk::SensorAbsent("CAN interface is not available");
        }
        ifreq request{};
        std::snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", interface_.c_str());
        if (ioctl(socket_, SIOCGIFINDEX, &request) < 0) {
            ::close(socket_);
            socket_ = -1;
            throw ogw::sdk::SensorAbsent("CAN interface is not available");
        }
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = request.ifr_ifindex;
        if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(socket_);
            socket_ = -1;
            throw std::runtime_error("cannot bind CAN interface");
        }
        timeval receive_timeout{};
        receive_timeout.tv_sec = stale_after_ms_ / 1000;
        receive_timeout.tv_usec = static_cast<decltype(receive_timeout.tv_usec)>((stale_after_ms_ % 1000) * 1000);
        ::setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
        const std::string device = std::string(R"({"localId":")") + frame_.deviceLocalId + R"(","displayName":")" +
                                   frame_.displayName +
                                   R"(","status":"disconnected","capabilities":[{"id":"encoder_counts","kind":"measurement","valueType":"number","displayName":"Encoder","unit":"counts","presentation":"sparkline","staleAfterMs":)" +
                                   std::to_string(stale_after_ms_) +
                                   R"(},{"id":"duty","kind":"measurement","valueType":"number","displayName":"Duty","unit":"%","presentation":"gauge","range":{"min":0,"max":100},"staleAfterMs":)" +
                                   std::to_string(stale_after_ms_) +
                                   R"(},{"id":"direction","kind":"measurement","valueType":"string","displayName":"Direction","presentation":"text","staleAfterMs":)" +
                                   std::to_string(stale_after_ms_) + "}]}";
        host().register_device(device);
        running_.store(true);
        thread_ = std::thread([this] { loop(); });
    }

    void stop() override {
        running_.store(false);
        if (socket_ >= 0) {
            ::shutdown(socket_, SHUT_RDWR);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        if (socket_ >= 0) {
            ::close(socket_);
            socket_ = -1;
        }
    }

private:
    void loop() {
        bool live = false;
        while (running_.load()) {
            can_frame frame{};
            const auto count = ::read(socket_, &frame, sizeof(frame));
            if (count < static_cast<ssize_t>(sizeof(frame.can_id))) {
                if (!running_.load()) {
                    break;
                }
                if (live) {
                    live = false;
                    host().update_device(std::string(R"({"localId":")") + frame_.deviceLocalId + R"(","status":"disconnected"})");
                }
                continue;
            }
            if ((frame.can_id & CAN_EFF_FLAG) != 0 || (frame.can_id & CAN_SFF_MASK) != frame_.id) {
                continue;
            }
            if (!live) {
                live = true;
                host().update_device(std::string(R"({"localId":")") + frame_.deviceLocalId + R"(","status":"connected"})");
            }
            for (const auto& signal : decode_frame(frame_, frame.data, frame.can_dlc)) {
                if (signal.capabilityId == "direction") {
                    const char* label = signal.value == 0 ? "CW" : "CCW";
                    auto reading = ogw::sdk::string_reading(label, OGW_QUALITY_GOOD, host().wall_time_ms(), nullptr);
                    host().publish(frame_.deviceLocalId, signal.capabilityId, reading);
                } else {
                    auto reading = ogw::sdk::number_reading(signal.value, OGW_QUALITY_GOOD, host().wall_time_ms(), nullptr);
                    host().publish(frame_.deviceLocalId, signal.capabilityId, reading);
                }
            }
        }
        host().update_device(std::string(R"({"localId":")") + frame_.deviceLocalId + R"(","status":"disconnected"})");
    }

    std::string interface_ = "can0";
    CanFrameMap frame_;
    int stale_after_ms_ = 1000;
    int socket_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace

OGW_EXPORT_PLUGIN(CanMotorPlugin)
