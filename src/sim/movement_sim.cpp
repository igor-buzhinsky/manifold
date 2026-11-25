#include "sim/movement_sim.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "game.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "party/party.hpp"
#include "physics/collision.hpp"
#include "util/asset_mapping.hpp"
#include "util/random.hpp"
#include "world/door.hpp"
#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Mixed into the seed of a level for the random input, so that it is independent of level generation.
constexpr std::uint64_t kInputSalt = 0x5eed'1e9b'7a1c'0f3dULL;
/// How long the same keys are held, in seconds.
constexpr float kInputPeriod = 0.7f;
/// The margin over GameConfig::run_speed that MovementViolation::kTooFast allows.
constexpr float kSpeedMargin = 1.01f;
/// The most frames per seed, so that their count fits an int.
constexpr float kMaxFrames = 1e9f;
/// How near a door must be, in metres from the center of its opening, for the simulation to open or close it.
constexpr float kDoorReach = 4.0f;
/// The chance that the nearest door is opened or closed when new keys are chosen.
constexpr double kDoorChance = 0.5;

/// New random keys: mostly forward, sometimes backward or neither (standing or only stepping aside), sometimes with a
/// step aside, a turn, a look up or down, running or a jump.
PlayerInput random_input(Rng& rng) {
    PlayerInput input;
    constexpr std::array<double, 3> kForwardWeights{0.7, 0.1, 0.2};  // Forward, backward, neither.
    constexpr std::array<float, 3> kForward{1.0f, -1.0f, 0.0f};
    input.forward = kForward.at(random_weighted_index(rng, kForwardWeights));
    if (random_chance(rng, 0.3)) {
        input.strafe = random_chance(rng, 0.5) ? 1.0f : -1.0f;
    }
    if (random_chance(rng, 0.5)) {
        input.turn = random_float(rng, -1.0f, 1.0f);
    }
    if (random_chance(rng, 0.3)) {
        input.look = random_float(rng, -1.0f, 1.0f);
    }
    input.run = random_chance(rng, 0.5);
    if (random_chance(rng, 0.2)) {
        input.rise = 1.0f;  // Only in the first frame of the period, see run_seed().
    }
    return input;
}

/// Starts to open the door nearest to the player, or to close it if it is open or opening, if it is closer than
/// kDoorReach. The game refuses to close a door on the player, and to move a door that is moving.
///
/// \return Whether a door started to move.
bool move_nearest_door(Game& game) {
    const std::span<const Door> doors = game.level().world.doors();
    const glm::vec2 player(game.player().position());
    std::optional<std::size_t> nearest;
    float nearest_distance = kDoorReach;
    for (std::size_t i = 0; i < doors.size(); ++i) {
        const float distance = glm::distance(glm::vec2(doors[i].base_center), player);
        if (distance < nearest_distance) {
            nearest = i;
            nearest_distance = distance;
        }
    }
    return nearest && game.set_door_open(*nearest, game.doors()[*nearest].target < 1.0f);
}

/// The deepest overlap of the player with the level, its obstacles, ramps and closed doors, in metres, or 0.
float deepest_overlap(const Game& game) {
    const World& world = game.level().world;
    const VerticalCylinder body = game.player().body();
    float deepest = 0.0f;
    const auto keep = [&deepest](const std::optional<Contact>& contact) {
        if (contact) {
            deepest = std::max(deepest, contact->depth);
        }
    };
    keep(find_contact(body, world.bounds()));
    const Box2 box{.min = glm::vec2(body.base) - body.radius, .max = glm::vec2(body.base) + body.radius};
    std::vector<std::size_t> indices;
    world.find_obstacles_near(box, indices);
    for (const std::size_t index : indices) {
        keep(find_contact(body, world.obstacles()[index]));
    }
    world.find_ramps_near(box, indices);
    for (const std::size_t index : indices) {
        keep(find_contact(body, world.ramps()[index]));
    }
    for (std::size_t i = 0; i < world.doors().size(); ++i) {
        if (!game.doors()[i].passable()) {
            keep(find_contact(body, world.doors()[i].blocker()));
        }
    }
    return deepest;
}

/// Collects the violations of one run, see run_movement_sim().
class Recorder {
public:
    /// \param report Where the violations are added. It must outlive the recorder.
    /// \param seed   The seed of the run.
    Recorder(MovementSimReport& report, std::size_t examples, std::uint64_t seed)
        : report_(&report), examples_(examples), seed_(seed) {}

    /// Counts a violation after a frame of `game`, and keeps it as an example if there are fewer of its kind yet.
    void add(MovementViolation violation, const Game& game, int frame, std::string detail) {
        int& count = report_->counts.at(static_cast<std::size_t>(violation));
        ++count;
        if (std::cmp_less_equal(count, examples_)) {
            report_->examples.push_back({.violation = violation,
                                         .layout = game.level().layout,
                                         .seed = seed_,
                                         .level_number = game.level_number(),
                                         .frame = frame,
                                         .position = game.player().position(),
                                         .detail = std::move(detail)});
        }
    }

private:
    MovementSimReport* report_;
    std::size_t examples_;
    std::uint64_t seed_;
};

/// A time in the air: from the frame that left the ground to the one that landed.
struct AirTime {
    bool in_air = false;   ///< Whether the player is in the air. The other fields hold only while they are.
    float start_z = 0.0f;  ///< The height before the frame that left the ground.
    bool jumped = false;   ///< Whether that frame jumped.
    int frames = 0;        ///< Frames in the air so far.
};

/// Checks the frame's landing or time in the air, see MovementViolation::kBriefGroundLoss and kLongFall.
///
/// \param[in,out] air    The time in the air before the frame. Updated for the frame.
/// \param was_on_ground  Whether the player was on the ground before the frame.
/// \param start_z        The player's height before the frame.
/// \param jumped         Whether the frame's input jumped.
/// \return Whether the player has been in the air too long, so the run of the seed stops.
bool check_air_time(AirTime& air, const Game& game, bool was_on_ground, float start_z, bool jumped, float dt, int frame,
                    Recorder& recorder, MovementSimReport& report) {
    const glm::vec3& position = game.player().position();
    if (game.player().on_ground()) {
        if (air.in_air && !air.jumped) {
            const float drop = air.start_z - position.z;
            if (drop >= kRealDrop) {
                ++report.real_drops;
            } else {
                recorder.add(MovementViolation::kBriefGroundLoss, game, frame,
                             std::format("{} frame(s) in the air, drop {:.4f} m", air.frames, drop));
            }
        }
        air = AirTime{};
        return false;
    }
    if (!air.in_air) {
        air = AirTime{.in_air = true, .start_z = start_z, .jumped = jumped && was_on_ground, .frames = 0};
    }
    ++air.frames;
    if (static_cast<float>(air.frames) * dt > kMaxAirTime) {
        recorder.add(MovementViolation::kLongFall, game, frame, std::format("{:.1f} s in the air", kMaxAirTime));
        return true;
    }
    return false;
}

/// Plays one seed of one layout, see run_movement_sim().
void run_seed(const MovementSimConfig& config, LayoutKind layout, std::uint64_t seed, MovementSimReport& report) {
    GameConfig game_config;
    // A portrait for any gender for each character, so that a new party logs no warning about missing portraits.
    game_config.level.assets.set(AssetKey::kAnyPortraits, static_cast<std::uint32_t>(game_config.party.size));
    Game game(game_config, seed, layout);
    Recorder recorder(report, config.examples, seed);
    Rng rng(seed ^ kInputSalt);
    const int frames = static_cast<int>(std::lround(config.seconds / config.dt));
    const int period = std::max(1, static_cast<int>(std::lround(kInputPeriod / config.dt)));
    const float max_step = (game_config.run_speed * kSpeedMargin * config.dt) + kMaxPush;
    PlayerInput keys;
    AirTime air;
    ++report.levels;
    for (int frame = 0; frame < frames; ++frame) {
        if (frame % period == 0) {
            keys = random_input(rng);
            if (random_chance(rng, kDoorChance) && move_nearest_door(game)) {
                ++report.door_moves;
            }
        }
        PlayerInput input = keys;
        if (frame % period != 0) {
            input.rise = 0.0f;  // A jump is a single press.
        }
        const bool was_on_ground = game.player().on_ground();
        const bool jumped = input.rise > 0.0f && was_on_ground;
        report.jumps += jumped ? 1 : 0;
        const glm::vec3 start = game.player().position();
        const int level_starts = game.level_starts();
        game.update(input, config.dt);
        ++report.frames;
        if (game.level_starts() != level_starts) {
            // The exit was reached: a new level, where the player starts on the floor.
            ++report.levels;
            air = AirTime{};
            continue;
        }
        const glm::vec3& position = game.player().position();
        const float step = glm::length(glm::vec2(position - start));
        if (step > max_step) {
            recorder.add(MovementViolation::kTooFast, game, frame,
                         std::format("{:.3f} m in a frame, {:.2f} m/s", step, step / config.dt));
        }
        const float overlap = deepest_overlap(game);
        if (overlap > kMaxOverlap) {
            recorder.add(MovementViolation::kOverlap, game, frame, std::format("{:.4f} m deep", overlap));
        }
        if (check_air_time(air, game, was_on_ground, start.z, jumped, config.dt, frame, recorder, report)) {
            return;
        }
    }
}

}  // namespace

std::string movement_violation_name(MovementViolation violation) {
    switch (violation) {
        case MovementViolation::kBriefGroundLoss:
            return "brief ground loss";
        case MovementViolation::kOverlap:
            return "overlap";
        case MovementViolation::kLongFall:
            return "long fall";
        case MovementViolation::kTooFast:
            return "too fast";
    }
    return "unknown";
}

bool MovementSimReport::ok() const {
    return std::ranges::all_of(counts, [](int count) { return count == 0; });
}

MovementSimReport run_movement_sim(const MovementSimConfig& config) {
    if (config.seeds < 1) {
        throw std::invalid_argument("the number of seeds must be positive");
    }
    if (!std::isfinite(config.dt) || config.dt <= 0.0f || config.dt > kInputPeriod / 2.0f) {
        throw std::invalid_argument(std::format("dt must be above 0 and at most {} s", kInputPeriod / 2.0f));
    }
    if (!std::isfinite(config.seconds) || config.seconds < config.dt || config.seconds / config.dt > kMaxFrames) {
        throw std::invalid_argument(std::format("seconds must give from 1 to {} frames", kMaxFrames));
    }
    MovementSimReport report;
    for (const LayoutKind layout : config.layouts) {
        for (int i = 0; i < config.seeds; ++i) {
            run_seed(config, layout, config.first_seed + static_cast<std::uint64_t>(i), report);
        }
    }
    return report;
}

std::string format_movement_sim_report(const MovementSimReport& report) {
    std::string text = std::format("{} frames, {} levels, {} jumps, {} real drops, {} door moves\n", report.frames,
                                   report.levels, report.jumps, report.real_drops, report.door_moves);
    for (std::size_t i = 0; i < kMovementViolationCount; ++i) {
        text +=
            std::format("{}: {}\n", movement_violation_name(static_cast<MovementViolation>(i)), report.counts.at(i));
    }
    for (const MovementSimExample& example : report.examples) {
        text += std::format("  {}: layout {}, seed {}, level {}, frame {}, at ({:.2f}, {:.2f}, {:.2f}): {}\n",
                            movement_violation_name(example.violation), layout_name(example.layout), example.seed,
                            example.level_number, example.frame, example.position.x, example.position.y,
                            example.position.z, example.detail);
    }
    text += report.ok() ? "OK\n" : "FAILED\n";
    return text;
}

}  // namespace manifold
