#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

namespace ogw {

// Single-threaded. The caller provides synchronization.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

    void push_drop_oldest(T item) {
        if (items_.size() >= capacity_) {
            items_.pop_front();
            ++dropped_;
        }
        items_.push_back(std::move(item));
    }

    std::optional<T> try_pop() {
        if (items_.empty()) {
            return std::nullopt;
        }
        T value = std::move(items_.front());
        items_.pop_front();
        return value;
    }

    bool empty() const { return items_.empty(); }
    std::size_t size() const { return items_.size(); }
    std::uint64_t dropped() const { return dropped_; }

private:
    std::size_t capacity_;
    std::uint64_t dropped_ = 0;
    std::deque<T> items_;
};

}  // namespace ogw
