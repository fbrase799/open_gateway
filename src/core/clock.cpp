#include "ogw/clock.hpp"

#include <chrono>

namespace ogw {

std::int64_t SystemClock::wall_time_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

ManualClock::ManualClock(std::int64_t start_ms) : now_(start_ms) {}

std::int64_t ManualClock::wall_time_ms() const {
    std::lock_guard lock(mu_);
    return now_;
}

void ManualClock::set(std::int64_t epoch_ms) {
    std::lock_guard lock(mu_);
    now_ = epoch_ms;
}

void ManualClock::advance(std::int64_t delta_ms) {
    std::lock_guard lock(mu_);
    now_ += delta_ms;
}

}  // namespace ogw
