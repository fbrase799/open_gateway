#include "ogw/application.hpp"

#include "ogw/json_codec.hpp"
#include "ogw/log.hpp"
#include "ogw/source_policy.hpp"
#include "ogw/version.hpp"

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

Application::Application(Config config)
    : config_(std::move(config)),
      pipeline_(registry_, clock_, config_.historySamples),
      plugins_(registry_, pipeline_, clock_, config_.pluginDir, config_.telemetryQueue) {}

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
    plugins_.set_listener([this](const RuntimeEvent& event) { hub_.broadcast(event_json(event, registry_, pipeline_)); });

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
    if (!plugins_.real_device_connected()) {
        for (std::size_t index = 0; index < config_.plugins.size(); ++index) {
            const auto& plugin = config_.plugins[index];
            if (!loaded[index] || !use_simulated_source(SourceRequest{plugin.simulated, plugin.activate}, false)) {
                continue;
            }
            auto status = plugins_.activate(plugin.id);
            if (!status.ok) {
                log_message(LogLevel::Warn, "plugins", status.error.empty() ? "plugin failed" : status.error,
                            {{"pluginId", plugin.id}});
            }
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
    plugins_.set_listener(nullptr);
    plugins_.shutdown();
    log_message(LogLevel::Info, "main", "open gateway stopped");
    return 0;
}

}  // namespace ogw
