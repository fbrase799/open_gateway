#pragma once

#include "ogw/pipeline.hpp"
#include "ogw/plugin_manager.hpp"
#include "ogw/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ogw {

std::string health_json(std::string_view version, std::uint32_t abi_version, std::int64_t uptime_ms);
std::string metrics_json(const PipelineCounters& counters, std::uint64_t dropped, std::uint64_t coalesced,
                         std::size_t active_devices, std::size_t active_plugins);
std::string devices_json(const std::vector<DeviceSnapshot>& devices, const TelemetryPipeline& pipeline,
                         std::size_t history_limit);
std::string device_json(const DeviceSnapshot& device, const TelemetryPipeline& pipeline, std::size_t history_limit);
std::string plugins_json(const std::vector<PluginInfo>& plugins, const DeviceRegistry& registry);
std::string current_json(const std::vector<DeviceSnapshot>& devices);
std::string history_json(const std::vector<HistorySample>& samples);
std::string event_json(const RuntimeEvent& event, const DeviceRegistry& registry, const TelemetryPipeline& pipeline);
std::string snapshot_json(const DeviceRegistry& registry, const TelemetryPipeline& pipeline,
                          const std::vector<PluginInfo>& plugins, const std::string& support_object);
std::string command_json(const CommandStatus& status, std::string_view state);

}  // namespace ogw
