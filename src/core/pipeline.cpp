#include "ogw/pipeline.hpp"

#include <algorithm>
#include <cstddef>

namespace ogw {
namespace {

std::string history_key(const std::string& device_id, const std::string& capability_id) {
    return device_id + "\n" + capability_id;
}

}  // namespace

TelemetryPipeline::TelemetryPipeline(DeviceRegistry& registry, Clock& clock, std::size_t history_samples)
    : registry_(registry), clock_(clock), history_samples_(history_samples == 0 ? 1 : history_samples) {}

TelemetryPipeline::Result TelemetryPipeline::publish(const std::string& device_id, const std::string& capability_id,
                                                     Reading reading) {
    Result result;
    auto published = registry_.publish(device_id, capability_id, std::move(reading), clock_.wall_time_ms());
    if (!published.accepted) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        result.reason = std::move(published.reason);
        return result;
    }
    accepted_.fetch_add(1, std::memory_order_relaxed);
    append_history(device_id, capability_id, published.stored);
    result.accepted = true;
    result.update = ReadingUpdate{device_id, capability_id, published.stored};
    return result;
}

std::vector<ReadingUpdate> TelemetryPipeline::mark_plugin_disconnected(std::string_view plugin_id) {
    std::vector<ReadingUpdate> updates;
    for (const auto& snapshot : registry_.mark_plugin_disconnected(plugin_id)) {
        for (const auto& reading : snapshot.readings) {
            updates.push_back(ReadingUpdate{snapshot.deviceId, reading.first, reading.second});
        }
        if (snapshot.readings.empty()) {
            updates.push_back(ReadingUpdate{snapshot.deviceId, {}, {}});
        }
    }
    return updates;
}

std::vector<ReadingUpdate> TelemetryPipeline::evaluate_staleness() {
    std::vector<ReadingUpdate> updates;
    for (const auto& entry : registry_.mark_stale(clock_.wall_time_ms())) {
        const auto split = entry.first.find('\n');
        if (split == std::string::npos) {
            continue;
        }
        updates.push_back(ReadingUpdate{entry.first.substr(0, split), entry.first.substr(split + 1), entry.second});
    }
    return updates;
}

void TelemetryPipeline::append_history(const std::string& device_id, const std::string& capability_id,
                                       const Reading& reading) {
    HistorySample sample;
    sample.value = reading.value;
    sample.quality = reading.quality;
    sample.sourceTimestampMs = reading.sourceTimestampMs;
    sample.receivedAtMs = reading.receivedAtMs;
    std::lock_guard lock(history_mu_);
    auto& ring = history_[history_key(device_id, capability_id)];
    ring.push_back(std::move(sample));
    while (ring.size() > history_samples_) {
        ring.pop_front();
    }
}

std::vector<HistorySample> TelemetryPipeline::history(const std::string& device_id, const std::string& capability_id,
                                                      std::size_t limit) const {
    return recent(device_id, capability_id, limit);
}

std::vector<HistorySample> TelemetryPipeline::recent(const std::string& device_id, const std::string& capability_id,
                                                     std::size_t limit) const {
    std::lock_guard lock(history_mu_);
    const auto it = history_.find(history_key(device_id, capability_id));
    if (it == history_.end() || limit == 0) {
        return {};
    }
    const auto& ring = it->second;
    const std::size_t count = std::min(limit, ring.size());
    return std::vector<HistorySample>(ring.end() - static_cast<std::ptrdiff_t>(count), ring.end());
}

PipelineCounters TelemetryPipeline::counters() const {
    PipelineCounters counters;
    counters.accepted = accepted_.load(std::memory_order_relaxed);
    counters.rejected = rejected_.load(std::memory_order_relaxed);
    return counters;
}

}  // namespace ogw
