#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

struct Endpoint {
    std::string host;
    std::string port;
};

Endpoint parse_endpoint(const char* url) {
    std::string text = url == nullptr ? "" : url;
    constexpr std::string_view http = "http://";
    if (text.rfind(http, 0) == 0) {
        text = text.substr(http.size());
    }
    const auto colon = text.find(':');
    Endpoint endpoint;
    if (colon == std::string::npos) {
        endpoint.host = text;
        endpoint.port = "80";
        return endpoint;
    }
    endpoint.host = text.substr(0, colon);
    endpoint.port = text.substr(colon + 1);
    const auto slash = endpoint.port.find('/');
    if (slash != std::string::npos) {
        endpoint.port = endpoint.port.substr(0, slash);
    }
    return endpoint;
}

struct HttpResult {
    int status = 0;
    std::string body;
};

HttpResult request(const Endpoint& endpoint, http::verb method, const std::string& target, const std::string& body = {}) {
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);
    stream.expires_after(std::chrono::seconds(5));
    stream.connect(resolver.resolve(endpoint.host, endpoint.port));
    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, endpoint.host);
    if (!body.empty()) {
        req.set(http::field::content_type, "application/json");
        req.body() = body;
        req.prepare_payload();
    }
    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);
    return {static_cast<int>(response.result_int()), response.body()};
}

bool wait_until(std::chrono::milliseconds budget, const std::function<bool()>& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return predicate();
}

class SocketLog {
public:
    explicit SocketLog(Endpoint endpoint) : endpoint_(std::move(endpoint)) {
        thread_ = std::thread([this] { run(); });
    }

    ~SocketLog() { stop(); }

    void stop() {
        stop_.store(true);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    bool contains(const std::string& needle) const {
        std::lock_guard lock(mu_);
        for (const auto& message : messages_) {
            if (message.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    void run() {
        try {
            net::io_context ioc;
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);
            stream.expires_after(std::chrono::seconds(5));
            stream.connect(resolver.resolve(endpoint_.host, endpoint_.port));
            websocket::stream<beast::tcp_stream> socket(std::move(stream));
            socket.handshake(endpoint_.host + ":" + endpoint_.port, "/api/v1/stream");
            while (!stop_.load()) {
                socket.next_layer().expires_after(std::chrono::milliseconds(400));
                beast::flat_buffer buffer;
                beast::error_code error;
                socket.read(buffer, error);
                if (error == beast::error::timeout) {
                    continue;
                }
                if (error) {
                    break;
                }
                std::lock_guard lock(mu_);
                messages_.push_back(beast::buffers_to_string(buffer.data()));
            }
        } catch (const std::exception&) {
        }
    }

    Endpoint endpoint_;
    std::atomic<bool> stop_{false};
    mutable std::mutex mu_;
    std::vector<std::string> messages_;
    std::thread thread_;
};

}  // namespace

TEST_CASE("runtime plugin activation and faults", "[live]") {
    const char* http_env = std::getenv("OGW_HTTP");
    const char* admin_env = std::getenv("OGW_ADMIN");
    if (http_env == nullptr || admin_env == nullptr) {
        SKIP("OGW_HTTP and OGW_ADMIN are required");
    }
    const Endpoint http = parse_endpoint(http_env);
    const Endpoint admin = parse_endpoint(admin_env);

    REQUIRE(wait_until(std::chrono::seconds(20), [&] {
        try {
            return request(http, http::verb::get, "/api/v1/health").status == 200;
        } catch (const std::exception&) {
            return false;
        }
    }));

    REQUIRE(wait_until(std::chrono::seconds(10), [&] {
        auto body = request(http, http::verb::get, "/api/v1/devices").body;
        return body.find("temperature-sim/room-1") != std::string::npos;
    }));

    auto support = nlohmann::json::parse(request(http, http::verb::get, "/api/v1/support").body);
    const auto support_text = support.dump();
    REQUIRE(support_text.find("\"rest\"") != std::string::npos);
    REQUIRE(support_text.find("\"websocket\"") != std::string::npos);
    REQUIRE(support_text.find("temperature-sim") != std::string::npos);
    REQUIRE(support_text.find("\"mqtt\"") == std::string::npos);
    REQUIRE(request(http, http::verb::put, "/api/v1/support/exports/mqtt", R"({"enabled":true})").status == 404);
    REQUIRE(request(http, http::verb::put, "/api/v1/support/sources/temperature-sim", R"({"state":"active","path":"/tmp/x"})").status ==
            400);
    REQUIRE(request(http, http::verb::put, "/api/v1/support/exports/websocket", R"({"enabled":false})").status == 200);
    REQUIRE(request(http, http::verb::get, "/api/v1/support").body.find("\"enabled\":false") != std::string::npos);
    REQUIRE(request(http, http::verb::get, "/api/v1/health").status == 200);
    REQUIRE(request(http, http::verb::put, "/api/v1/support/exports/websocket", R"({"enabled":true})").status == 200);

    auto reading = request(http, http::verb::get, "/api/v1/telemetry/current");
    REQUIRE(reading.status == 200);
    auto current = nlohmann::json::parse(reading.body);
    REQUIRE(current.contains("readings"));
    REQUIRE_FALSE(current["readings"].empty());
    REQUIRE(current["readings"][0].contains("quality"));
    REQUIRE(current["readings"][0].contains("sourceTimestamp"));

    SocketLog stream(http);
    REQUIRE(wait_until(std::chrono::seconds(5), [&] { return stream.contains("temperature-sim/room-1"); }));

    const char* volume = std::getenv("OGW_PLUGIN_VOLUME");
    const char* motor = std::getenv("OGW_MOTOR_SO");
    const char* bad = std::getenv("OGW_BAD_SO");
    REQUIRE(volume != nullptr);
    REQUIRE(motor != nullptr);
    REQUIRE(bad != nullptr);
    std::filesystem::copy_file(motor, std::filesystem::path(volume) / "libmotor_sim.so",
                               std::filesystem::copy_options::overwrite_existing);
    auto scanned = request(admin, http::verb::post, "/api/v1/admin/plugins/scan");
    REQUIRE(scanned.status == 200);
    auto activated = request(admin, http::verb::post, "/api/v1/admin/plugins/motor-sim/activate");
    REQUIRE(activated.status == 200);

    REQUIRE(wait_until(std::chrono::seconds(8), [&] {
        return stream.contains("device_upsert") && stream.contains("motor-sim/motor-01");
    }));
    auto devices = nlohmann::json::parse(request(http, http::verb::get, "/api/v1/devices").body);
    REQUIRE(devices.dump().find("motor-sim/motor-01") != std::string::npos);
    REQUIRE(devices.dump().find("temperature-sim/room-1") != std::string::npos);

    const auto accepted_before = nlohmann::json::parse(request(http, http::verb::get, "/api/v1/metrics").body)["readingsAccepted"].get<std::uint64_t>();
    REQUIRE(request(admin, http::verb::post, "/api/v1/admin/plugins/motor-sim/deactivate").status == 200);
    REQUIRE(wait_until(std::chrono::seconds(5), [&] {
        auto motor_device = request(http, http::verb::get, "/api/v1/devices/motor-sim/motor-01");
        return motor_device.status == 200 && motor_device.body.find("\"disconnected\"") != std::string::npos;
    }));
    REQUIRE(wait_until(std::chrono::seconds(5), [&] {
        const auto accepted = nlohmann::json::parse(request(http, http::verb::get, "/api/v1/metrics").body)["readingsAccepted"].get<std::uint64_t>();
        return accepted > accepted_before;
    }));

    std::filesystem::copy_file(bad, std::filesystem::path(volume) / "libbad_plugin.so",
                               std::filesystem::copy_options::overwrite_existing);
    REQUIRE(request(admin, http::verb::post, "/api/v1/admin/plugins/scan").status == 200);
    auto plugins = nlohmann::json::parse(request(http, http::verb::get, "/api/v1/plugins").body);
    bool failed = false;
    bool temperature_active = false;
    for (const auto& plugin : plugins["plugins"]) {
        if (plugin["id"] == "bad-plugin") {
            failed = plugin["state"] == "failed";
        }
        if (plugin["id"] == "temperature-sim" && plugin["state"] == "active") {
            temperature_active = true;
        }
    }
    REQUIRE(failed);
    REQUIRE(temperature_active);
    REQUIRE(request(http, http::verb::get, "/api/v1/health").status == 200);

    REQUIRE(wait_until(std::chrono::seconds(30), [&] {
        auto body = request(http, http::verb::get, "/api/v1/devices/temperature-sim/room-1").body;
        return body.find("\"disconnected\"") != std::string::npos || body.find("\"bad\"") != std::string::npos ||
               body.find("\"faulted\"") != std::string::npos;
    }));
}
