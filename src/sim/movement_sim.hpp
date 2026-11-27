#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "game.hpp"
#include "level/level.hpp"

namespace manifold {

/// What a movement simulation runs, see run_movement_sim().
struct MovementSimConfig {
    std::uint64_t first_seed = 1;  ///< Seed of the first level of each layout.
    int seeds = 40;                ///< How many seeds, from first_seed on, for each layout.
    /// The layouts to run each seed with.
    std::vector<LayoutKind> layouts = {LayoutKind::kDebug, LayoutKind::kRooms};
    float seconds = 30.0f;     ///< Game time per seed, in seconds.
    float dt = 1.0f / 60.0f;   ///< Duration of a frame, in seconds.
    std::size_t examples = 5;  ///< How many examples to keep of each kind of violation.
    /// The settings of the game, such as the speeds and the size of the player and the layouts, e.g. from the data
    /// files, see load_game_data(). Its `level.assets` are replaced by a portrait for any gender for each character,
    /// so that a new party logs no warning.
    GameConfig game;
};

/// A kind of broken invariant of the player's movement.
enum class MovementViolation : std::uint8_t {
    /// The player left the ground without a jump and landed less than kRealDrop lower, e.g. because collision lost
    /// the floor for a few frames.
    kBriefGroundLoss,
    /// After a frame, the player overlapped the level, an obstacle, a ramp or a closed door by more than kMaxOverlap.
    kOverlap,
    /// The player was in the air for more than kMaxAirTime, e.g. falling through the floor. The rest of that seed is
    /// skipped.
    kLongFall,
    /// The player moved farther horizontally in a frame than GameConfig::run_speed allows, with a margin of 1%, plus
    /// kMaxPush for a push out of the geometry, e.g. off the side of a ramp.
    kTooFast,
};

/// How many kinds MovementViolation has.
inline constexpr std::size_t kMovementViolationCount = 4;

/// A drop of at least this, in metres, from leaving the ground to landing, is a real fall, not a kBriefGroundLoss.
inline constexpr float kRealDrop = 0.01f;
/// The deepest overlap allowed after a frame, in metres, see MovementViolation::kOverlap.
inline constexpr float kMaxOverlap = 1e-3f;
/// How far a push out of the geometry may move the player horizontally in a frame, in metres, on top of running, see
/// MovementViolation::kTooFast. E.g. running along the low side of a ramp, a little up its slope, can push the player
/// 2 to 3 cm off it in a frame. A speed bug, such as twice the running speed, moves the player much farther.
inline constexpr float kMaxPush = 0.05f;
/// The longest time in the air allowed, in seconds, see MovementViolation::kLongFall. A jump is in the air for about
/// 0.64 s, and a fall from 10 m for about 1.4 s.
inline constexpr float kMaxAirTime = 3.0f;

/// The name of a kind of violation, as the report shows it, e.g. "brief ground loss".
std::string movement_violation_name(MovementViolation violation);

/// One broken invariant, where and when it happened.
struct MovementSimExample {
    MovementViolation violation = MovementViolation::kOverlap;
    /// The layout of the level it happened in. After an exit, it can differ from the run's first level, since the
    /// next levels choose their layouts at random.
    LayoutKind layout = LayoutKind::kDebug;
    std::uint64_t seed = 0;    ///< The seed of the run, which is that of its first level.
    int level_number = 0;      ///< How many levels came before the one it happened in, in the run.
    int frame = 0;             ///< Counted from the start of the seed's run.
    glm::vec3 position{0.0f};  ///< The player's position after the frame.
    std::string detail;        ///< E.g. how deep the overlap was.
};

/// What a movement simulation found.
struct MovementSimReport {
    long frames = 0;     ///< Frames simulated, over all seeds and layouts.
    int levels = 0;      ///< Levels played, including the next levels reached through an exit.
    int jumps = 0;       ///< Jumps made.
    int real_drops = 0;  ///< Falls of at least kRealDrop without a jump, e.g. off the side of a ramp.
    int door_moves = 0;  ///< Doors that the simulation started to open or close near the player.
    /// How often each kind of violation happened, indexed by MovementViolation.
    std::array<int, kMovementViolationCount> counts{};
    /// The first MovementSimConfig::examples of each kind, in the order they happened.
    std::vector<MovementSimExample> examples;

    /// Whether no invariant was broken.
    bool ok() const;
};

/// Plays levels with random input and checks invariants of the player's movement after every frame.
///
/// For each layout and seed, a game starts at that seed and the player walks, runs, steps aside, stands, turns, looks
/// up and down and jumps at random, with new keys every 0.7 s, for MovementSimConfig::seconds. Each time, the door
/// nearest to the player, if one is within 4 m, starts to open or close with a chance of 50%, unless the game refuses.
/// A level whose exit is reached is followed by the next one, as in the game. The random choices depend only on the
/// seed, so a run can be repeated. It checks the invariants of MovementViolation.
///
/// \throws std::invalid_argument if `config.seeds` is not positive, `config.dt` is not above 0 and at most half of the
///         0.7 s between new keys (so that a jump is a new press), or `config.seconds` gives no frame or more than
///         1e9 frames per seed.
MovementSimReport run_movement_sim(const MovementSimConfig& config);

/// The report as text: the totals, the count of each violation and the examples, with the layout, the seed of the run,
/// the level number and the frame. An example in the first level (level 0) can be looked at in the game with
/// `manifold SEED --layout LAYOUT` and `teleport`; a later level is reached only by running the simulation again.
std::string format_movement_sim_report(const MovementSimReport& report);

}  // namespace manifold
