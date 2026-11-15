#include "ogw/mqtt_client.hpp"

#include "ogw/log.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ogw {
namespace {

void append_u16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>(value & 0xff));
}

void append_mqtt_string(std::string& out, std::string_view value) {
    append_u16(out, static_cast<std::uint16_t>(value.size()));
    out.append(value);
}

void append_remaining(std::string& out, std::size_t length) {
    do {
        auto byte = static_cast<unsigned char>(length % 128);
        length /= 128;
        if (length > 0) {
            byte = static_cast<unsigned char>(byte | 0x80);
        }
        out.push_back(static_cast<char>(byte));
    } while (length > 0);
}

std::string packet(unsigned char header, const std::string& body) {
    std::string out;
    out.push_back(static_cast<char>(header));
    append_remaining(out, body.size());
    out.append(body);
    return out;
}

bool send_all(int fd, const std::string& bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto count = ::send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
        if (count <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

struct Endpoint {
    std::string host = "127.0.0.1";
    std::string port = "1883";
};

Endpoint parse_mqtt_url(const std::string& url) {
    std::string text = url;
    constexpr std::string_view prefix = "mqtt://";
    if (text.rfind(prefix, 0) == 0) {
        text = text.substr(prefix.size());
    }
    const auto slash = text.find('/');
    if (slash != std::string::npos) {
        text = text.substr(0, slash);
    }
    Endpoint endpoint;
    const auto colon = text.rfind(':');
    if (colon == std::string::npos) {
        endpoint.host = text;
        return endpoint;
    }
    endpoint.host = text.substr(0, colon);
    endpoint.port = text.substr(colon + 1);
    return endpoint;
}

int connect_broker(const Endpoint& endpoint) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(endpoint.host.c_str(), endpoint.port.c_str(), &hints, &result) != 0) {
        return -1;
    }
    int fd = -1;
    for (auto* item = result; item != nullptr; item = item->ai_next) {
        fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (fd < 0) {
            continue;
        }
        timeval timeout{};
        timeout.tv_sec = 3;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (::connect(fd, item->ai_addr, item->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    return fd;
}

struct Publish {
    std::string topic;
    std::string payload;
    bool retained = false;
};

bool parse_command(const std::string& topic, std::string& device_id, std::string& capability_id) {
    constexpr std::string_view prefix = "ogw/v1/devices/";
    constexpr std::string_view marker = "/commands/";
    if (topic.rfind(prefix, 0) != 0) {
        return false;
    }
    const auto pos = topic.find(marker);
    if (pos == std::string::npos || pos <= prefix.size()) {
        return false;
    }
    device_id = topic.substr(prefix.size(), pos - prefix.size());
    capability_id = topic.substr(pos + marker.size());
    return !device_id.empty() && !capability_id.empty() && capability_id.find('/') == std::string::npos;
}

}  // namespace

struct MqttClient::Impl {
    explicit Impl(std::string url) : url_(std::move(url)) {}

    void start() {
        if (started_.exchange(true)) {
            return;
        }
        if (url_.empty()) {
            return;
        }
        stop_.store(false);
        worker_ = std::thread([this] { run(); });
    }

    void stop() {
        stop_.store(true);
        cv_.notify_all();
        {
            std::lock_guard lock(mu_);
            if (fd_ >= 0) {
                ::shutdown(fd_, SHUT_RDWR);
            }
        }
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        started_.store(false);
    }

    void publish(std::string topic, std::string payload, bool retained) {
        if (url_.empty()) {
            const auto count = skipped_.fetch_add(1);
            if (count == 0) {
                log_message(LogLevel::Info, "mqtt", "broker url is empty; publishes are counted and not sent");
            }
            return;
        }
        {
            std::lock_guard lock(mu_);
            if (outbound_.size() >= 64) {
                outbound_.pop_front();
            }
            outbound_.push_back(Publish{std::move(topic), std::move(payload), retained});
            cv_.notify_one();
        }
    }

    void run() {
        const Endpoint endpoint = parse_mqtt_url(url_);
        std::string inbound;
        while (!stop_.load()) {
            const int fd = connect_broker(endpoint);
            if (fd < 0) {
                wait_retry();
                continue;
            }
            {
                std::lock_guard lock(mu_);
                fd_ = fd;
            }
            std::string connect_body;
            append_mqtt_string(connect_body, "MQTT");
            connect_body.push_back(4);
            connect_body.push_back(0x02);
            append_u16(connect_body, 30);
            append_mqtt_string(connect_body, "ogw-gateway");
            std::string subscribe;
            append_u16(subscribe, 1);
            append_mqtt_string(subscribe, "ogw/v1/devices/#");
            subscribe.push_back(0);
            if (!send_all(fd, packet(0x10, connect_body)) || !wait_connack(fd, inbound) ||
                !send_all(fd, packet(0x82, subscribe))) {
                close_fd();
                inbound.clear();
                wait_retry();
                continue;
            }
            log_message(LogLevel::Info, "mqtt", "connected", {{"url", url_}});
            auto last = std::chrono::steady_clock::now();
            bool failed = false;
            while (!stop_.load() && !failed) {
                pollfd watch{};
                watch.fd = fd;
                watch.events = POLLIN;
                const int ready = ::poll(&watch, 1, 200);
                if (ready < 0) {
                    failed = true;
                    break;
                }
                if (ready > 0 && (watch.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                    failed = true;
                    break;
                }
                if (ready > 0 && (watch.revents & POLLIN) != 0) {
                    char buffer[1024];
                    const auto count = ::recv(fd, buffer, sizeof(buffer), 0);
                    if (count <= 0) {
                        failed = true;
                        break;
                    }
                    inbound.append(buffer, buffer + count);
                    last = std::chrono::steady_clock::now();
                    if (!drain_inbound(inbound)) {
                        failed = true;
                        break;
                    }
                }
                if (!flush_outbound(fd)) {
                    failed = true;
                    break;
                }
                if (std::chrono::steady_clock::now() - last > std::chrono::seconds(15)) {
                    const std::string ping{static_cast<char>(0xC0), 0};
                    if (!send_all(fd, ping)) {
                        failed = true;
                        break;
                    }
                    last = std::chrono::steady_clock::now();
                }
            }
            close_fd();
            inbound.clear();
            if (!stop_.load()) {
                wait_retry();
            }
        }
        close_fd();
    }

    bool drain_inbound(std::string& inbound) {
        std::string frame;
        while (take_frame(inbound, frame)) {
            const auto header = static_cast<unsigned char>(frame[0]);
            if ((header & 0xF0) != 0x30) {
                continue;
            }
            std::size_t offset = 1;
            std::size_t remaining = 0;
            std::size_t multiplier = 1;
            while (offset < frame.size()) {
                const auto byte = static_cast<unsigned char>(frame[offset++]);
                remaining += static_cast<std::size_t>(byte & 0x7f) * multiplier;
                multiplier *= 128;
                if ((byte & 0x80) == 0) {
                    break;
                }
            }
            if (offset + 2 > frame.size()) {
                return false;
            }
            const auto topic_len = (static_cast<unsigned char>(frame[offset]) << 8) |
                                   static_cast<unsigned char>(frame[offset + 1]);
            offset += 2;
            if (offset + topic_len > frame.size()) {
                return false;
            }
            const std::string topic = frame.substr(offset, topic_len);
            offset += topic_len;
            std::string device_id;
            std::string capability_id;
            if (!parse_command(topic, device_id, capability_id)) {
                continue;
            }
            const std::string payload = offset < frame.size() ? frame.substr(offset) : std::string{};
            CommandHandler handler;
            {
                std::lock_guard lock(mu_);
                handler = handler_;
            }
            if (handler) {
                try {
                    handler(device_id, capability_id, payload);
                } catch (const std::exception& ex) {
                    log_message(LogLevel::Warn, "mqtt", ex.what());
                }
            }
            (void)remaining;
        }
        return true;
    }

    static bool take_frame(std::string& inbound, std::string& frame) {
        if (inbound.size() < 2) {
            return false;
        }
        std::size_t offset = 1;
        std::size_t length = 0;
        std::size_t multiplier = 1;
        while (offset < inbound.size() && offset < 5) {
            const auto byte = static_cast<unsigned char>(inbound[offset++]);
            length += static_cast<std::size_t>(byte & 0x7f) * multiplier;
            if ((byte & 0x80) == 0) {
                if (inbound.size() < offset + length) {
                    return false;
                }
                frame = inbound.substr(0, offset + length);
                inbound.erase(0, offset + length);
                return true;
            }
            multiplier *= 128;
        }
        return false;
    }

    bool flush_outbound(int fd) {
        std::deque<Publish> batch;
        {
            std::lock_guard lock(mu_);
            batch.swap(outbound_);
        }
        for (const auto& item : batch) {
            std::string body;
            append_mqtt_string(body, item.topic);
            body.append(item.payload);
            const auto header = static_cast<unsigned char>(item.retained ? 0x31 : 0x30);
            if (!send_all(fd, packet(header, body))) {
                return false;
            }
        }
        return true;
    }

    bool wait_connack(int fd, std::string& inbound) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline && !stop_.load()) {
            if (inbound.size() >= 4) {
                break;
            }
            pollfd watch{};
            watch.fd = fd;
            watch.events = POLLIN;
            if (::poll(&watch, 1, 200) <= 0) {
                continue;
            }
            char buffer[64];
            const auto count = ::recv(fd, buffer, sizeof(buffer), 0);
            if (count <= 0) {
                return false;
            }
            inbound.append(buffer, buffer + count);
        }
        std::string frame;
        if (!take_frame(inbound, frame) || frame.empty() || static_cast<unsigned char>(frame[0]) != 0x20) {
            return false;
        }
        return frame.size() >= 4 && static_cast<unsigned char>(frame.back()) == 0;
    }

    void close_fd() {
        std::lock_guard lock(mu_);
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    void wait_retry() {
        std::unique_lock lock(mu_);
        cv_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_.load(); });
    }

    std::string url_;
    CommandHandler handler_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Publish> outbound_;
    int fd_ = -1;
    std::thread worker_;
    std::atomic<bool> started_{false};
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> skipped_{0};
};

MqttClient::MqttClient(std::string url) : impl_(std::make_unique<Impl>(std::move(url))) {}

MqttClient::~MqttClient() { stop(); }

void MqttClient::set_command_handler(CommandHandler handler) {
    std::lock_guard lock(impl_->mu_);
    impl_->handler_ = std::move(handler);
}

void MqttClient::start() { impl_->start(); }

void MqttClient::stop() {
    if (impl_) {
        impl_->stop();
    }
}

void MqttClient::publish(std::string topic, std::string payload, bool retained) {
    impl_->publish(std::move(topic), std::move(payload), retained);
}

}  // namespace ogw
