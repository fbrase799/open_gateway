#pragma once

#include "ogw/clock.hpp"
#include "ogw/registry.hpp"
#include "ogw/types.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ogw {

struct ReadingUpdate {
    std::string deviceId;
    std::string capabilityId;
    Reading reading;
};

struct PipelineCounters {
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
};

class TelemetryPipeline {
public:
    TelemetryPipeline(DeviceRegistry& registry, Clock& clock, std::size_t history_samples);

    struct Result {
        bool accepted = false;
        std::string reason;
        ReadingUpdate update;
    };

    Result publish(const std::string& device_id, const std::string& capability_id, Reading reading);
    std::vector<ReadingUpdate> mark_plugin_disconnected(std::string_view plugin_id);
    std::vector<ReadingUpdate> evaluate_staleness();

    std::vector<HistorySample> history(const std::string& device_id, const std::string& capability_id,
                                       std::size_t limit) const;
    std::vector<HistorySample> recent(const std::string& device_id, const std::string& capability_id,
                                      std::size_t limit) const;

    PipelineCounters counters() const;

private:
    void append_history(const std::string& device_id, const std::string& capability_id, const Reading& reading);

    DeviceRegistry& registry_;
    Clock& clock_;
    std::size_t history_samples_;
    mutable std::mutex history_mu_;
    std::unordered_map<std::string, std::deque<HistorySample>> history_;
    std::atomic<std::uint64_t> accepted_{0};
    std::atomic<std::uint64_t> rejected_{0};
};

}  // namespace ogw
