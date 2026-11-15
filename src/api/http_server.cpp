#include "ogw/http_server.hpp"

#include "ogw/json_codec.hpp"
#include "ogw/log.hpp"
#include "ogw/plugin_abi.h"
#include "ogw/version.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace ogw {
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

std::size_t query_limit(const std::string& query, std::size_t fallback) {
    const auto pos = query.find("limit=");
    if (pos == std::string::npos) {
        return fallback;
    }
    auto number = query.substr(pos + 6);
    const auto end = number.find('&');
    if (end != std::string::npos) {
        number = number.substr(0, end);
    }
    try {
        auto value = static_cast<std::size_t>(std::stoul(number));
        if (value == 0) {
            value = 1;
        }
        if (value > 5000) {
            value = 5000;
        }
        return value;
    } catch (const std::exception&) {
        return fallback;
    }
}

void apply_cors(http::response<http::string_body>& response, const ApiContext& context) {
    if (context.config != nullptr && !context.config->corsOrigin.empty()) {
        response.set(http::field::access_control_allow_origin, context.config->corsOrigin);
        response.set(http::field::access_control_allow_headers, "Content-Type");
        response.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    }
}

http::response<http::string_body> respond(http::status status, std::string body, const ApiContext& context,
                                          const char* content_type = "application/json") {
    http::response<http::string_body> response{status, 11};
    response.set(http::field::server, "open-gateway");
    response.set(http::field::content_type, content_type);
    response.set(http::field::cache_control, "no-store");
    apply_cors(response, context);
    response.body() = std::move(body);
    response.prepare_payload();
    response.keep_alive(false);
    return response;
}

http::response<http::string_body> error_response(http::status status, const char* code, const std::string& message,
                                                 const ApiContext& context) {
    nlohmann::json document = {{"error", {{"code", code}, {"message", message}}}};
    return respond(status, document.dump(), context);
}

std::string plugin_state(const ApiContext& context, const std::string& id) {
    for (const auto& plugin : context.plugins->list()) {
        if (plugin.id == id) {
            return std::string(to_string(plugin.state));
        }
    }
    return {};
}

http::response<http::string_body> handle_admin(const http::request<http::string_body>& request, const std::string& path,
                                               const ApiContext& context) {
    if (request.method() != http::verb::post) {
        return error_response(http::status::method_not_allowed, "method_not_allowed", "POST is required", context);
    }
    if (path == "/api/v1/admin/plugins/scan") {
        auto status = context.plugins->scan();
        if (!status.ok) {
            return error_response(http::status::bad_request, "scan_failed", status.error, context);
        }
        return respond(http::status::ok, plugins_json(context.plugins->list(), *context.registry), context);
    }
    constexpr std::string_view prefix = "/api/v1/admin/plugins/";
    if (path.rfind(prefix, 0) != 0) {
        return error_response(http::status::not_found, "not_found", "unknown admin route", context);
    }
    const auto rest = path.substr(prefix.size());
    const auto slash = rest.rfind('/');
    if (slash == std::string::npos) {
        return error_response(http::status::not_found, "not_found", "unknown admin route", context);
    }
    const auto id = rest.substr(0, slash);
    const auto action = rest.substr(slash + 1);
    CommandStatus status;
    if (action == "activate") {
        status = context.plugins->activate(id);
    } else if (action == "deactivate") {
        status = context.plugins->deactivate(id);
    } else {
        return error_response(http::status::not_found, "not_found", "unknown admin route", context);
    }
    if (!status.ok && status.error == "unknown plugin") {
        return error_response(http::status::not_found, "not_found", status.error, context);
    }
    if (!status.ok) {
        return error_response(http::status::conflict, "plugin_error", status.error, context);
    }
    return respond(http::status::ok, command_json(status, plugin_state(context, status.id)), context);
}

bool rejects_upload(const nlohmann::json& body) {
    if (!body.is_object()) {
        return false;
    }
    for (const auto& key : {"path", "file", "url", "bytes"}) {
        if (body.contains(key)) {
            return true;
        }
    }
    if (body.contains("settings") && body["settings"].is_object()) {
        const auto& settings = body["settings"];
        for (const auto& key : {"path", "file", "url", "bytes"}) {
            if (settings.contains(key)) {
                return true;
            }
        }
    }
    return false;
}

bool rest_open(const ApiContext& context) {
    return context.plugins != nullptr && context.plugins->export_enabled("rest");
}

http::response<http::string_body> handle_support_put(const http::request<http::string_body>& request, const std::string& path,
                                                     ApiContext& context) {
    if (request.method() != http::verb::put) {
        return error_response(http::status::method_not_allowed, "method_not_allowed", "PUT is required", context);
    }
    const auto body = nlohmann::json::parse(request.body(), nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        return error_response(http::status::bad_request, "invalid_setting", "JSON object is required", context);
    }
    if (rejects_upload(body)) {
        return error_response(http::status::bad_request, "upload_rejected", "plugin upload is not accepted", context);
    }
    constexpr std::string_view exports = "/api/v1/support/exports/";
    constexpr std::string_view sources = "/api/v1/support/sources/";
    if (path.rfind(exports, 0) == 0) {
        if (!body.contains("enabled") || !body["enabled"].is_boolean()) {
            return error_response(http::status::bad_request, "invalid_setting", "enabled must be a boolean", context);
        }
        auto status = context.plugins->set_export(path.substr(exports.size()), body["enabled"].get<bool>());
        if (status.error == "not_advertised") {
            return error_response(http::status::not_found, "not_advertised", "export is not advertised", context);
        }
        if (!status.ok) {
            return error_response(http::status::bad_request, status.error.c_str(), status.error, context);
        }
        if (path.substr(exports.size()) == "websocket" && !body["enabled"].get<bool>() && context.hub != nullptr) {
            context.hub->close_all();
        }
        return respond(http::status::ok, context.plugins->support_json(), context);
    }
    if (path.rfind(sources, 0) == 0) {
        if (!body.contains("state") || !body["state"].is_string()) {
            return error_response(http::status::bad_request, "invalid_setting", "state must be active or inactive", context);
        }
        const auto settings = body.contains("settings") ? body["settings"].dump() : "{}";
        auto status = context.plugins->configure_source(path.substr(sources.size()), body["state"].get<std::string>(), settings);
        if (status.error == "not_advertised") {
            return error_response(http::status::not_found, "not_advertised", "source is not advertised", context);
        }
        if (status.error == "invalid_setting" || !status.ok) {
            return error_response(http::status::bad_request, "invalid_setting",
                                  status.error.empty() ? "setting was rejected" : status.error, context);
        }
        return respond(http::status::ok, context.plugins->support_json(), context);
    }
    return error_response(http::status::not_found, "not_found", "unknown route", context);
}

http::response<http::string_body> handle_command(const http::request<http::string_body>& request, const std::string& path,
                                                 ApiContext& context) {
    if (!rest_open(context)) {
        return error_response(http::status::conflict, "export_disabled", "REST export is disabled", context);
    }
    constexpr std::string_view prefix = "/api/v1/devices/";
    constexpr std::string_view marker = "/commands/";
    const auto rest = path.substr(prefix.size());
    const auto marker_pos = rest.rfind(marker);
    if (marker_pos == std::string::npos || marker_pos == 0) {
        return error_response(http::status::not_found, "not_found", "unknown route", context);
    }
    const auto device_id = rest.substr(0, marker_pos);
    const auto capability_id = rest.substr(marker_pos + marker.size());
    if (capability_id.empty() || capability_id.find('/') != std::string::npos) {
        return error_response(http::status::not_found, "not_found", "unknown route", context);
    }
    const auto body = nlohmann::json::parse(request.body(), nullptr, false);
    if (body.is_discarded() || !body.is_object() || !body.contains("value") || !body["value"].is_number()) {
        return error_response(http::status::bad_request, "invalid_setting", "value must be a number", context);
    }
    auto status = context.plugins->invoke(device_id, capability_id, body.dump());
    if (status.error == "unknown_device" || status.error == "unknown_capability") {
        return error_response(http::status::not_found, "not_found", status.error, context);
    }
    if (status.error == "not_a_command") {
        return error_response(http::status::bad_request, "not_a_command", "capability is not a command", context);
    }
    if (!status.ok) {
        return error_response(http::status::conflict, status.error.empty() ? "rejected" : status.error.c_str(),
                              status.error.empty() ? "command was rejected" : status.error, context);
    }
    return respond(http::status::ok, R"({"ok":true})", context);
}

http::response<http::string_body> handle_public(const http::request<http::string_body>& request, const std::string& path,
                                                const std::string& query, ApiContext& context) {
    if (request.method() == http::verb::options) {
        return respond(http::status::no_content, "", context);
    }
    if (path.rfind("/api/v1/admin/", 0) == 0) {
        return error_response(http::status::not_found, "not_found", "unknown route", context);
    }
    if (path == "/api/v1/support" && request.method() == http::verb::get) {
        return respond(http::status::ok, context.plugins->support_json(), context);
    }
    if (path.rfind("/api/v1/support/", 0) == 0) {
        return handle_support_put(request, path, context);
    }
    constexpr std::string_view devices = "/api/v1/devices/";
    if (request.method() == http::verb::post && path.rfind(devices, 0) == 0 && path.find("/commands/") != std::string::npos) {
        return handle_command(request, path, context);
    }
    if (request.method() != http::verb::get) {
        return error_response(http::status::method_not_allowed, "method_not_allowed", "GET is required", context);
    }
    if (path == "/api/v1/health") {
        const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                  context.started)
                                .count();
        return respond(http::status::ok, health_json(kVersion, OGW_PLUGIN_ABI_VERSION, uptime), context);
    }
    if (!rest_open(context) && path.rfind("/api/v1/", 0) == 0) {
        return error_response(http::status::conflict, "export_disabled", "REST export is disabled", context);
    }
    if (path == "/api/v1/plugins") {
        return respond(http::status::ok, plugins_json(context.plugins->list(), *context.registry), context);
    }
    if (path == "/api/v1/metrics") {
        std::size_t active_plugins = 0;
        for (const auto& plugin : context.plugins->list()) {
            if (plugin.state == PluginState::Active) {
                ++active_plugins;
            }
        }
        return respond(http::status::ok,
                       metrics_json(context.pipeline->counters(), context.plugins->telemetry_dropped(),
                                    context.hub->coalesced(), context.registry->size(), active_plugins),
                       context);
    }
    if (path == "/api/v1/telemetry/current") {
        return respond(http::status::ok, current_json(context.registry->list()), context);
    }
    if (path == "/api/v1/devices") {
        return respond(http::status::ok, devices_json(context.registry->list(), *context.pipeline, 60), context);
    }
    constexpr std::string_view prefix = "/api/v1/devices/";
    if (path.rfind(prefix, 0) == 0) {
        const auto rest = path.substr(prefix.size());
        constexpr std::string_view marker = "/capabilities/";
        const auto marker_pos = rest.find(marker);
        if (marker_pos == std::string::npos) {
            auto device = context.registry->find(rest);
            if (!device) {
                return error_response(http::status::not_found, "not_found", "unknown device", context);
            }
            return respond(http::status::ok, device_json(*device, *context.pipeline, 60), context);
        }
        const auto device_id = rest.substr(0, marker_pos);
        const auto after = rest.substr(marker_pos + marker.size());
        const auto slash = after.find('/');
        if (slash == std::string::npos || after.substr(slash) != "/history") {
            return error_response(http::status::not_found, "not_found", "unknown route", context);
        }
        const auto capability_id = after.substr(0, slash);
        if (!context.registry->find(device_id)) {
            return error_response(http::status::not_found, "not_found", "unknown device", context);
        }
        const auto samples = context.pipeline->history(device_id, capability_id, query_limit(query, 60));
        return respond(http::status::ok, history_json(samples), context);
    }
    return error_response(http::status::not_found, "not_found", "unknown route", context);
}

}  // namespace

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, ApiContext context)
        : ws_(std::move(socket)), context_(context) {}

    void run(http::request<http::string_body> request) {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& response) {
            response.set(http::field::server, "open-gateway");
        }));
        auto self = shared_from_this();
        ws_.async_accept(request, [self](beast::error_code ec) {
            if (ec) {
                return;
            }
            self->ws_.text(true);
            self->context_.hub->join(self);
            self->send(self->context_.hub->snapshot_json());
            self->do_read();
        });
    }

    void request_close() {
        bool start = false;
        {
            std::lock_guard lock(mu_);
            if (closed_) {
                return;
            }
            closed_ = true;
            outbound_.clear();
            if (!writing_) {
                writing_ = true;
                start = true;
            }
        }
        if (start) {
            do_close();
        }
    }

    void send(std::string message) {
        bool need_snapshot = false;
        {
            std::lock_guard lock(mu_);
            if (closed_) {
                return;
            }
            if (outbound_.size() >= 256) {
                need_snapshot = true;
            }
        }
        if (need_snapshot) {
            message = context_.hub->snapshot_json();
            context_.hub->note_coalesced();
        }
        bool start = false;
        {
            std::lock_guard lock(mu_);
            if (closed_) {
                return;
            }
            if (need_snapshot) {
                outbound_.clear();
            }
            outbound_.push_back(std::move(message));
            if (!writing_) {
                writing_ = true;
                start = true;
            }
        }
        if (start) {
            do_write();
        }
    }

private:
    void do_read() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->context_.hub->leave(self.get());
                std::lock_guard lock(self->mu_);
                self->closed_ = true;
                return;
            }
            self->buffer_.consume(self->buffer_.size());
            self->do_read();
        });
    }

    void do_write() {
        {
            std::lock_guard lock(mu_);
            if (outbound_.empty()) {
                writing_ = false;
                return;
            }
            write_buffer_ = outbound_.front();
        }
        auto self = shared_from_this();
        ws_.async_write(net::buffer(write_buffer_), [self](beast::error_code ec, std::size_t) {
            if (ec) {
                self->context_.hub->leave(self.get());
                std::lock_guard lock(self->mu_);
                self->closed_ = true;
                self->writing_ = false;
                return;
            }
            bool more = false;
            {
                std::lock_guard lock(self->mu_);
                if (!self->outbound_.empty()) {
                    self->outbound_.pop_front();
                }
                more = !self->outbound_.empty();
                if (!more) {
                    self->writing_ = false;
                }
            }
            if (more) {
                self->do_write();
            } else if (self->closed_) {
                self->do_close();
            }
        });
    }

    void do_close() {
        auto self = shared_from_this();
        ws_.async_close(websocket::close_code::normal, [self](beast::error_code) {
            self->context_.hub->leave(self.get());
            std::lock_guard lock(self->mu_);
            self->writing_ = false;
        });
    }

    websocket::stream<tcp::socket> ws_;
    ApiContext context_;
    beast::flat_buffer buffer_;
    std::mutex mu_;
    std::deque<std::string> outbound_;
    std::string write_buffer_;
    bool writing_ = false;
    bool closed_ = false;
};

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket, ApiContext context, bool admin)
        : stream_(std::move(socket)), context_(context), admin_(admin) {}

    void run() { do_read(); }

private:
    void do_read() {
        parser_.emplace();
        parser_->body_limit(1024 * 1024);
        stream_.expires_after(std::chrono::seconds(30));
        auto self = shared_from_this();
        http::async_read(stream_, buffer_, *parser_, [self](beast::error_code ec, std::size_t) {
            if (ec) {
                return;
            }
            self->request_ = self->parser_->release();
            auto target = std::string(self->request_.target());
            std::string query;
            const auto query_pos = target.find('?');
            if (query_pos != std::string::npos) {
                query = target.substr(query_pos + 1);
                target.erase(query_pos);
            }
            if (!self->admin_ && websocket::is_upgrade(self->request_) && target == "/api/v1/stream") {
                if (self->context_.plugins == nullptr || !self->context_.plugins->export_enabled("websocket")) {
                    self->response_ = error_response(http::status::conflict, "export_disabled", "WebSocket export is disabled",
                                                     self->context_);
                    auto write_self = self->shared_from_this();
                    http::async_write(self->stream_, self->response_, [write_self](beast::error_code, std::size_t) {
                        beast::error_code shutdown_ec;
                        write_self->stream_.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
                    });
                    return;
                }
                self->stream_.expires_never();
                auto session = std::make_shared<WsSession>(self->stream_.release_socket(), self->context_);
                session->run(std::move(self->request_));
                return;
            }
            self->response_ = self->admin_ ? handle_admin(self->request_, target, self->context_)
                                           : handle_public(self->request_, target, query, self->context_);
            self->response_.keep_alive(false);
            auto write_self = self->shared_from_this();
            http::async_write(self->stream_, self->response_, [write_self](beast::error_code write_ec, std::size_t) {
                beast::error_code shutdown_ec;
                write_self->stream_.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
                (void)write_ec;
            });
        });
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::optional<http::request_parser<http::string_body>> parser_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    ApiContext context_;
    bool admin_;
};

void WsHub::set_context(ApiContext* context) { context_ = context; }

void WsHub::join(const std::shared_ptr<WsSession>& session) {
    std::lock_guard lock(mu_);
    sessions_.push_back(session);
}

void WsHub::leave(WsSession* session) {
    std::lock_guard lock(mu_);
    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                   [&](const std::weak_ptr<WsSession>& item) {
                                       auto locked = item.lock();
                                       return locked == nullptr || locked.get() == session;
                                   }),
                    sessions_.end());
}

void WsHub::broadcast(std::string message) {
    std::vector<std::shared_ptr<WsSession>> sessions;
    {
        std::lock_guard lock(mu_);
        sessions.reserve(sessions_.size());
        std::vector<std::weak_ptr<WsSession>> alive;
        for (auto& item : sessions_) {
            if (auto locked = item.lock()) {
                sessions.push_back(locked);
                alive.push_back(item);
            }
        }
        sessions_.swap(alive);
    }
    for (const auto& session : sessions) {
        session->send(message);
    }
}

std::string WsHub::snapshot_json() const {
    if (context_ == nullptr || context_->registry == nullptr || context_->pipeline == nullptr ||
        context_->plugins == nullptr) {
        return R"({"type":"snapshot","devices":[],"plugins":[]})";
    }
    return ogw::snapshot_json(*context_->registry, *context_->pipeline, context_->plugins->list(),
                              context_->plugins->support_json());
}

void WsHub::close_all() {
    std::vector<std::shared_ptr<WsSession>> sessions;
    {
        std::lock_guard lock(mu_);
        for (auto& item : sessions_) {
            if (auto locked = item.lock()) {
                sessions.push_back(locked);
            }
        }
        sessions_.clear();
    }
    for (const auto& session : sessions) {
        session->request_close();
    }
}

std::uint64_t WsHub::coalesced() const { return coalesced_.load(std::memory_order_relaxed); }

void WsHub::note_coalesced() { coalesced_.fetch_add(1, std::memory_order_relaxed); }

HttpServer::HttpServer(net::io_context& ioc, std::uint16_t port, bool admin, ApiContext context)
    : acceptor_(ioc), admin_(admin), context_(context) {
    beast::error_code ec;
    acceptor_.open(tcp::v4(), ec);
    if (ec) {
        throw std::system_error(ec);
    }
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::system_error(ec);
    }
    acceptor_.bind(tcp::endpoint(tcp::v4(), port), ec);
    if (ec) {
        throw std::system_error(ec);
    }
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::system_error(ec);
    }
}

void HttpServer::start() {
    log_message(LogLevel::Info, "http", admin_ ? "admin listening" : "public listening",
                {{"port", std::to_string(acceptor_.local_endpoint().port())}});
    do_accept();
}

void HttpServer::do_accept() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<HttpSession>(std::move(socket), context_, admin_)->run();
        }
        if (acceptor_.is_open()) {
            do_accept();
        }
    });
}

}  // namespace ogw
