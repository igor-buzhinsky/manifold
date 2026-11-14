#include "util/frame_times.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

namespace manifold {

void FrameTimes::add(float seconds) {
    if (values_.size() < kCapacity) {
        values_.push_back(seconds);
        return;
    }
    values_[next_] = seconds;
    next_ = (next_ + 1) % kCapacity;
}

std::vector<float> FrameTimes::values() const {
    std::vector<float> ordered(values_.begin() + static_cast<std::ptrdiff_t>(next_), values_.end());
    ordered.insert(ordered.end(), values_.begin(), values_.begin() + static_cast<std::ptrdiff_t>(next_));
    return ordered;
}

float FrameTimes::average() const {
    if (values_.empty()) {
        return 0.0f;
    }
    return std::accumulate(values_.begin(), values_.end(), 0.0f) / static_cast<float>(values_.size());
}

float FrameTimes::longest() const { return values_.empty() ? 0.0f : std::ranges::max(values_); }

}  // namespace manifold
