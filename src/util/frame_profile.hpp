#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace manifold {

/// A part of a frame whose duration FrameProfile measures.
enum class FramePhase : std::uint8_t {
    kFrame,   ///< The whole frame, from the start of one to the start of the next.
    kUpdate,  ///< Game logic: input, Game::update(), sounds and the like, before drawing.
    kDraw,    ///< Issuing the OpenGL calls of the picture on the CPU, without waiting for the GPU.
    kGpu,     ///< The GPU's work on the picture, measured with timer queries.
    kSwap,    ///< Showing the picture: waiting for the GPU to finish and, with vsync, for the screen.
};

/// How many kinds of FramePhase there are.
inline constexpr std::size_t kFramePhaseCount = 5;

/// The name of a phase in the summary of FrameProfile, e.g. "update".
std::string_view frame_phase_name(FramePhase phase);

/// Statistics of the durations of one phase over many frames, in seconds.
struct DurationStats {
    std::size_t count = 0;  ///< How many durations there were. The other fields are 0 if none.
    float mean = 0.0f;
    float median = 0.0f;
    float p95 = 0.0f;  ///< 95% of the durations are at most this long.
    float max = 0.0f;
};

/// Computes the statistics of durations, in seconds. Percentiles are those of the nearest rank.
DurationStats compute_duration_stats(std::span<const float> durations);

/// Measures how long the phases of a number of frames take, for the `profile` console command.
///
/// The main loop adds the duration of each phase of each frame while the profile runs, then prints summary().
/// GPU durations come in later than the others, since the GPU works behind the CPU, so each phase has its own count.
class FrameProfile {
public:
    /// \param frames How many frames to measure, at least 1.
    explicit FrameProfile(std::size_t frames);

    /// Adds the duration of a phase of a frame, in seconds. Durations of kFrame count the frames.
    void add(FramePhase phase, float seconds);

    /// Whether as many frames were measured as asked for.
    bool done() const { return durations(FramePhase::kFrame).size() >= frames_; }

    /// The durations of a phase added so far, in seconds.
    std::span<const float> durations(FramePhase phase) const;

    /// A table with a line per phase that has durations: the mean, median, 95th percentile and longest
    /// duration in milliseconds, and frames per second from the mean frame time. `context` is put in the first line,
    /// e.g. the size of the picture.
    std::string summary(std::string_view context) const;

private:
    std::size_t frames_;
    std::array<std::vector<float>, kFramePhaseCount> durations_;
};

}  // namespace manifold
