#pragma once

#include "ogw/clock.hpp"
#include "ogw/config.hpp"
#include "ogw/pipeline.hpp"
#include "ogw/plugin_manager.hpp"
#include "ogw/registry.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ogw {

class WsSession;

struct ApiContext {
    const Config* config = nullptr;
    DeviceRegistry* registry = nullptr;
    TelemetryPipeline* pipeline = nullptr;
    PluginManager* plugins = nullptr;
    class WsHub* hub = nullptr;
    Clock* clock = nullptr;
    std::chrono::steady_clock::time_point started{};
};

class WsHub {
public:
    void set_context(ApiContext* context);
    void join(const std::shared_ptr<WsSession>& session);
    void leave(WsSession* session);
    void broadcast(std::string message);
    std::string snapshot_json() const;
    void close_all();
    std::uint64_t coalesced() const;
    void note_coalesced();

private:
    ApiContext* context_ = nullptr;
    std::mutex mu_;
    std::vector<std::weak_ptr<WsSession>> sessions_;
    std::atomic<std::uint64_t> coalesced_{0};
};

class HttpServer {
public:
    HttpServer(boost::asio::io_context& ioc, std::uint16_t port, bool admin, ApiContext context);
    void start();

private:
    void do_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    bool admin_;
    ApiContext context_;
};

}  // namespace ogw
