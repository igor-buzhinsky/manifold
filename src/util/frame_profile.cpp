#include "util/frame_profile.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace manifold {

std::string_view frame_phase_name(FramePhase phase) {
    switch (phase) {
        case FramePhase::kFrame:
            return "frame";
        case FramePhase::kUpdate:
            return "update";
        case FramePhase::kDraw:
            return "draw";
        case FramePhase::kGpu:
            return "gpu";
        case FramePhase::kSwap:
            return "swap";
    }
    return "?";
}

DurationStats compute_duration_stats(std::span<const float> durations) {
    if (durations.empty()) {
        return {};
    }
    std::vector<float> sorted(durations.begin(), durations.end());
    std::ranges::sort(sorted);
    // The nearest rank: the smallest value that at least this fraction of the values doesn't exceed.
    const auto percentile = [&sorted](float fraction) {
        const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<float>(sorted.size())));
        return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
    };
    return {.count = sorted.size(),
            .mean = std::accumulate(sorted.begin(), sorted.end(), 0.0f) / static_cast<float>(sorted.size()),
            .median = percentile(0.5f),
            .p95 = percentile(0.95f),
            .max = sorted.back()};
}

FrameProfile::FrameProfile(std::size_t frames) : frames_(std::max<std::size_t>(frames, 1)) {}

void FrameProfile::add(FramePhase phase, float seconds) {
    durations_[static_cast<std::size_t>(phase)].push_back(seconds);
}

std::span<const float> FrameProfile::durations(FramePhase phase) const {
    return durations_[static_cast<std::size_t>(phase)];
}

std::string FrameProfile::summary(std::string_view context) const {
    const DurationStats frame = compute_duration_stats(durations(FramePhase::kFrame));
    std::string text =
        std::format("Profile of {} frames, {}: {:.1f} FPS\n{:<8}{:>8}{:>8}{:>8}{:>8}  (ms)", frame.count, context,
                    frame.mean > 0.0f ? 1.0f / frame.mean : 0.0f, "phase", "mean", "median", "p95", "max");
    for (std::size_t i = 0; i < kFramePhaseCount; ++i) {
        const auto phase = static_cast<FramePhase>(i);
        const DurationStats stats = compute_duration_stats(durations(phase));
        if (stats.count == 0) {
            continue;
        }
        constexpr float kMs = 1000.0f;
        text += std::format("\n{:<8}{:>8.2f}{:>8.2f}{:>8.2f}{:>8.2f}", frame_phase_name(phase), stats.mean * kMs,
                            stats.median * kMs, stats.p95 * kMs, stats.max * kMs);
    }
    return text;
}

}  // namespace manifold
