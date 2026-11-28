#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <glad/gl.h>

namespace manifold {

/// Measures how long the GPU takes for the OpenGL commands between begin() and end(), e.g. for drawing a frame.
///
/// The GPU works behind the CPU, so results come a frame or two later, from collect(). A few queries are used in turn,
/// so that measuring never waits for the GPU. A frame whose query is still busy when its turn comes again isn't
/// measured.
///
/// Needs a current OpenGL context for its whole lifetime.
class GpuTimer {
public:
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    GpuTimer(GpuTimer&&) = delete;
    GpuTimer& operator=(GpuTimer&&) = delete;

    /// Starts measuring. Must be followed by end() before the next begin().
    void begin();

    /// Stops measuring what began with begin().
    void end();

    /// Durations in seconds of the measurements that the GPU finished since the last call, oldest first.
    std::vector<float> collect();

    /// Drops all measurements, also those the GPU hasn't finished, which it waits for, so that collect() returns only
    /// what begins after this. Must not be called between begin() and end().
    void reset();

private:
    static constexpr std::size_t kQueries = 4;

    std::array<GLuint, kQueries> queries_{};
    std::array<bool, kQueries> pending_{};  ///< Whether a query was ended and its result not yet collected.
    std::size_t next_ = 0;                  ///< The query for the next begin().
    bool measuring_ = false;                ///< Whether begin() started a query that end() hasn't ended.
};

}  // namespace manifold
