#pragma once

#include <cstddef>
#include <vector>

namespace manifold {

/// The durations of the last frames, e.g. for the graph of the debug overlay.
class FrameTimes {
public:
    /// How many frames are kept: about two seconds at 60 frames per second. Older ones are dropped.
    static constexpr std::size_t kCapacity = 120;

    /// Adds the duration of a frame, in seconds.
    void add(float seconds);

    /// The kept durations in seconds, oldest first.
    std::vector<float> values() const;

    /// The mean of the kept durations in seconds, or 0 if there are none.
    float average() const;

    /// The longest kept duration in seconds, or 0 if there are none.
    float longest() const;

private:
    std::vector<float> values_;  ///< A ring buffer of at most kCapacity values.
    std::size_t next_ = 0;       ///< Where the next value goes once the buffer is full.
};

}  // namespace manifold
