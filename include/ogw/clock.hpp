#pragma once

#include <cstdint>
#include <mutex>

namespace ogw {

class Clock {
public:
    virtual ~Clock() = default;
    virtual std::int64_t wall_time_ms() const = 0;
};

class SystemClock final : public Clock {
public:
    std::int64_t wall_time_ms() const override;
};

class ManualClock final : public Clock {
public:
    explicit ManualClock(std::int64_t start_ms = 0);
    std::int64_t wall_time_ms() const override;
    void set(std::int64_t epoch_ms);
    void advance(std::int64_t delta_ms);

private:
    mutable std::mutex mu_;
    std::int64_t now_;
};

}  // namespace ogw
