#include "ogw/application.hpp"

#include "ogw/json_codec.hpp"
#include "ogw/log.hpp"
#include "ogw/source_policy.hpp"
#include "ogw/version.hpp"

#include <nlohmann/json.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

#include <csignal>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace ogw {
namespace {

std::string measurement_payload(const DeviceSnapshot& device, const std::string& capability_id) {
    const Capability* capability = nullptr;
    for (const auto& item : device.capabilities) {
        if (item.id == capability_id) {
            capability = &item;
            break;
        }
    }
    if (capability == nullptr || capability->kind != "measurement") {
        return {};
    }
    const Reading* reading = nullptr;
    for (const auto& item : device.readings) {
        if (item.first == capability_id) {
            reading = &item.second;
            break;
        }
    }
    if (reading == nullptr) {
        return {};
    }
    nlohmann::json document = {{"deviceId", device.deviceId},
                               {"capabilityId", capability_id},
                               {"quality", std::string(to_string(reading->quality))},
                               {"sourceTimestampMs", reading->sourceTimestampMs}};
    std::visit([&document](const auto& value) { document["value"] = value; }, reading->value);
    return document.dump();
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      pipeline_(registry_, clock_, config_.historySamples),
      plugins_(registry_, pipeline_, clock_, config_.pluginDir, config_.telemetryQueue),
      mqtt_(config_.mqttUrl) {}

int Application::run(bool headless) {
    set_log_level(config_.logLevel);
    log_message(LogLevel::Info, "main", "open gateway starting",
                {{"version", std::string(kVersion)}, {"pluginDir", config_.pluginDir}});

    ApiContext context;
    context.config = &config_;
    context.registry = &registry_;
    context.pipeline = &pipeline_;
    context.plugins = &plugins_;
    context.hub = &hub_;
    context.clock = &clock_;
    context.started = std::chrono::steady_clock::now();
    hub_.set_context(&context);
    mqtt_.set_command_handler([this](const std::string& device_id, const std::string& capability_id, const std::string& payload) {
        if (!plugins_.export_enabled("mqtt")) {
            return;
        }
        plugins_.invoke(device_id, capability_id, payload);
    });
    mqtt_.start();
    plugins_.set_listener([this](const RuntimeEvent& event) {
        hub_.broadcast(event_json(event, registry_, pipeline_));
        if (!plugins_.export_enabled("mqtt")) {
            return;
        }
        if (event.kind == RuntimeEventKind::Reading) {
            const auto device = registry_.find(event.deviceId);
            if (!device) {
                return;
            }
            const auto payload = measurement_payload(*device, event.capabilityId);
            if (payload.empty()) {
                return;
            }
            mqtt_.publish("ogw/v1/devices/" + event.deviceId + "/capabilities/" + event.capabilityId, payload, false);
            return;
        }
        if (event.kind == RuntimeEventKind::DeviceUpsert) {
            const auto device = registry_.find(event.deviceId);
            if (!device) {
                return;
            }
            nlohmann::json document = {{"deviceId", device->deviceId},
                                       {"displayName", device->displayName},
                                       {"status", std::string(to_string(device->status))}};
            mqtt_.publish("ogw/v1/devices/" + event.deviceId, document.dump(), true);
        }
    });

    std::filesystem::create_directories(config_.pluginDir);
    std::vector<bool> loaded(config_.plugins.size(), false);
    for (std::size_t index = 0; index < config_.plugins.size(); ++index) {
        const auto& plugin = config_.plugins[index];
        auto status = plugins_.load_configured(plugin.file, plugin.id, plugin.configJson, false, plugin.simulated);
        loaded[index] = status.ok;
        if (!status.ok) {
            log_message(LogLevel::Warn, "plugins", status.error.empty() ? "plugin failed" : status.error,
                        {{"pluginId", plugin.id}, {"file", plugin.file}});
        }
    }
    for (std::size_t index = 0; index < config_.plugins.size(); ++index) {
        const auto& plugin = config_.plugins[index];
        if (!loaded[index] || !probe_source(SourceRequest{plugin.simulated, plugin.activate})) {
            continue;
        }
        auto status = plugins_.activate(plugin.id);
        if (!status.ok && status.error != "not_present") {
            log_message(LogLevel::Warn, "plugins", status.error.empty() ? "plugin failed" : status.error,
                        {{"pluginId", plugin.id}});
        }
    }

    boost::asio::io_context ioc(2);
    auto timer = std::make_shared<boost::asio::steady_timer>(ioc);
    std::function<void()> schedule;
    schedule = [&, timer] {
        timer->expires_after(std::chrono::seconds(1));
        timer->async_wait([&, timer](const boost::system::error_code& ec) {
            if (ec) {
                return;
            }
            plugins_.evaluate_staleness();
            if (headless) {
                std::cout << devices_json(registry_.list(), pipeline_, 0) << '\n';
            }
            schedule();
        });
    };
    schedule();

    std::unique_ptr<HttpServer> http;
    std::unique_ptr<HttpServer> admin;
    if (!headless) {
        http = std::make_unique<HttpServer>(ioc, config_.httpPort, false, context);
        http->start();
        if (config_.adminApi) {
            admin = std::make_unique<HttpServer>(ioc, config_.adminPort, true, context);
            admin->start();
        }
    }

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) { ioc.stop(); });

    std::vector<std::thread> threads;
    threads.reserve(2);
    for (int index = 0; index < 2; ++index) {
        threads.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    mqtt_.stop();
    plugins_.set_listener(nullptr);
    plugins_.shutdown();
    log_message(LogLevel::Info, "main", "open gateway stopped");
    return 0;
}

}  // namespace ogw
