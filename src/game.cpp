#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "physics/collision.hpp"
#include "physics/raycast.hpp"
#include "save/saved_game.hpp"
#include "world/door.hpp"
#include "world/obstacle.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// A horizontal push out of the world geometry longer than this, in metres, means that running ran into something.
/// Ramps and floors push straight up, so it is only rounding error there.
constexpr float kRunBlockedCorrection = 1e-5f;

/// The player's settings, moved to the start of `level` and turned the way it says.
PlayerParams at_start(PlayerParams params, const Level& level) {
    params.position = level.player_start;
    params.yaw = level.player_yaw;
    return params;
}

/// Generates the level of a game, see generate_level(). The first level of a game gets a torch on the free wall side of
/// the start room nearest to where the player starts, if there is one, since the party has no light of its own until it
/// casts Candlelight.
///
/// \param level_number How many levels came before it: 0 for the first level.
Level generate_game_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance,
                          std::uint64_t seed, int level_number) {
    return generate_level(config, layout, clearance, seed, level_number == 0);
}

/// Mixed into the seed of a game's first level for its party, which keeps the party independent of the random numbers
/// used by level generation.
constexpr std::uint64_t kPartySeedSalt = 0x8bb84b93962eacc9ULL;

}  // namespace

Game::Game(const GameConfig& config, std::uint64_t seed, std::optional<LayoutKind> layout)
    : Game(config, generate_game_level(config.level, layout, config.player.radius, seed, 0), seed) {
    given_layout_ = layout;
}

Game::Game(const GameConfig& config, Level level, std::uint64_t seed)
    : level_config_(config.level),
      player_params_(config.player),
      turn_speed_(config.turn_speed),
      look_speed_(config.look_speed),
      walk_speed_(config.walk_speed),
      run_speed_(config.run_speed),
      max_snap_slope_(config.max_snap_slope),
      jump_speed_(std::sqrt(2.0f * kEarthGravity * config.jump_height)),
      portraits_(config.portraits),
      party_config_(config.party),
      seed_(seed),
      level_(std::move(level)),
      player_(at_start(config.player, level_)),
      party_(make_new_party(seed)),
      level_start_party_(party_),
      doors_(level_.world.doors().size()) {}

std::vector<Character> Game::make_new_party(std::uint64_t seed) const {
    return make_party(portraits_, party_config_, seed ^ kPartySeedSalt);
}

void Game::set_hp(std::size_t index, int hp) {
    Character& character = party_.at(index);
    character.hp = std::clamp(hp, 0, character.max_hp);
}

void Game::teleport(const glm::vec3& position, std::optional<float> yaw) {
    player_.set_position(position);
    player_.set_vertical_speed(0.0f);
    player_.set_on_ground(false);
    footsteps_ = 0;
    step_progress_ = 1.0f - kFirstStepFraction;
    if (yaw) {
        player_.rotate(*yaw - player_.yaw(), 0.0f);
    }
}

void Game::next_level() {
    const std::uint64_t seed = next_level_seed(seed_);
    start_level(generate_game_level(level_config_, std::nullopt, player_params_.radius, seed, level_number_ + 1), seed,
                level_number_ + 1, std::nullopt);
}

void Game::new_game(std::uint64_t seed, std::optional<LayoutKind> layout) {
    Level level = generate_game_level(level_config_, layout, player_params_.radius, seed, 0);
    time_ = 0.0;
    party_ = make_new_party(seed);
    magic_ = PartyMagic{};
    start_level(std::move(level), seed, 0, layout);
}

void Game::load(const SavedGame& save) {
    Level level = generate_game_level(level_config_, save.layout, player_params_.radius, save.seed, save.level_number);
    time_ = save.time;
    party_ = save.party;
    magic_ = save.magic;
    start_level(std::move(level), save.seed, save.level_number, save.layout);
}

SavedGame Game::save_point() const {
    return {.seed = seed_,
            .level_number = level_number_,
            .layout = given_layout_,
            .time = level_start_time_,
            .party = level_start_party_,
            .magic = level_start_magic_};
}

void Game::start_level(Level level, std::uint64_t seed, int level_number, std::optional<LayoutKind> layout) {
    level_ = std::move(level);
    seed_ = seed;
    given_layout_ = layout;
    level_number_ = level_number;
    ++level_starts_;
    level_start_time_ = time_;
    level_start_party_ = party_;
    level_start_magic_ = magic_;
    player_ = Player(at_start(player_params_, level_));
    doors_.assign(level_.world.doors().size(), DoorState{});
    door_moves_.clear();
    footsteps_ = 0;
    step_progress_ = 1.0f - kFirstStepFraction;
}

float Game::animation_time() const { return static_cast<float>(std::fmod(time_, kAnimationPeriod)); }

void Game::update(const PlayerInput& input, float dt) {
    time_ += dt;
    magic_.advance(dt);
    for (const Spell spell : input.casts) {
        cast(spell);
    }
    for (DoorState& door : doors_) {
        door.advance(dt);
    }
    move_player(input, dt, door_blockers());
    const bool activate_pressed = input.activate && !activate_held_;
    activate_held_ = input.activate;
    if (activate_pressed) {
        activate();
    }
    if (input.click) {
        activate(input.click);
    }
    const std::optional<LevelExit>& exit = level_.world.exit();
    if (exit && exit->touches(player_.body())) {
        next_level();
    }
}

Activation Game::activate(std::optional<glm::vec3> direction) {
    const Ray view{.origin = player_.eye_position(), .direction = direction.value_or(player_.view_direction())};
    const std::optional<RayHit> hit = cast_ray(level_.world, doors_, view, kActivationReach);
    if (!hit || hit->target == RayTarget::kWorld) {
        return Activation::kNothing;
    }
    // A door that isn't passable is closed or moving, and opening it is refused while it moves.
    const bool open = !doors_.at(hit->door).passable();
    if (!set_door_open(hit->door, open)) {
        return Activation::kRefused;
    }
    return open ? Activation::kOpened : Activation::kClosed;
}

bool Game::set_door_open(std::size_t index, bool open) {
    DoorState& door = doors_.at(index);
    const float target = open ? 1.0f : 0.0f;
    if (door.moving() || door.target == target) {
        return false;
    }
    if (!open && find_contact(player_.body(), level_.world.doors()[index].blocker())) {
        return false;
    }
    door.target = target;
    door_moves_.push_back({.door = index, .open = open});
    return true;
}

std::vector<DoorMove> Game::take_door_moves() { return std::exchange(door_moves_, {}); }

std::vector<Obstacle> Game::door_blockers() const {
    std::vector<Obstacle> blockers;
    const std::span<const Door> doors = level_.world.doors();
    for (std::size_t i = 0; i < doors.size(); ++i) {
        if (!doors_[i].passable()) {
            blockers.push_back(doors[i].blocker());
        }
    }
    return blockers;
}

void Game::move_player(const PlayerInput& input, float dt, std::span<const Obstacle> blockers) {
    const bool rise_pressed = input.rise > 0.0f && !rise_held_;
    rise_held_ = input.rise > 0.0f;
    player_.rotate(input.turn * turn_speed_ * dt, input.look * look_speed_ * dt);

    // Walking is horizontal, in the direction the player faces, whatever the pitch.
    // Walking diagonally is not faster than walking straight.
    glm::vec2 direction{input.forward, input.strafe};
    const float length = glm::length(direction);
    if (length > 1.0f) {
        direction /= length;
    }
    const glm::vec3 forward{std::cos(player_.yaw()), std::sin(player_.yaw()), 0.0f};
    const glm::vec3 right{forward.y, -forward.x, 0.0f};
    const glm::vec3 walk = ((direction.x * forward) + (direction.y * right)) * walk_speed_ * dt;
    // Running only adds speed forward, in the horizontal direction of the view.
    bool running = input.run && direction.x > 0.0f && !levitating();
    glm::vec3 displacement = running ? walk + (direction.x * (run_speed_ - walk_speed_) * dt * forward) : walk;
    if (noclip_) {
        displacement.z = input.rise * walk_speed_ * dt;
        player_.set_position(player_.position() + displacement);
        // When noclip is turned off in the air, the fall starts from rest.
        player_.set_vertical_speed(0.0f);
        player_.set_on_ground(false);
        return;
    }
    if (levitating()) {
        // As with noclip, but stopped by what is in the way. Without gravity there is nothing to stand on, so there
        // are no footsteps and no sticking to ramps, and when the spell ends the fall starts from rest.
        displacement.z = input.rise * walk_speed_ * dt;
        player_.set_position(move_and_slide(level_.world, player_.body(), displacement, blockers).position);
        player_.set_vertical_speed(0.0f);
        player_.set_on_ground(false);
        return;
    }

    float start_speed = player_.vertical_speed();
    if (rise_pressed && player_.on_ground()) {
        // The extra half step of gravity makes up for the position lagging behind the speed in the update
        // below, so the peak is the same at any frame rate.
        start_speed = jump_speed_ + (level_.gravity * dt / 2.0f);
    }
    // Speed changes before the position (semi-implicit Euler), which keeps falls stable.
    const float vertical_speed = start_speed - (level_.gravity * dt);
    displacement.z = vertical_speed * dt;
    const glm::vec3 start = player_.position();
    const bool was_on_ground = player_.on_ground();
    SlideResult result = move_and_slide(level_.world, player_.body(), displacement, blockers);
    // Running into something, such as a wall or a door, moves only at walking speed, also along it. Ramps and floors
    // push the player straight up, so running on them goes on.
    if (running && glm::length(glm::vec2(result.correction)) > kRunBlockedCorrection) {
        running = false;
        displacement = glm::vec3(glm::vec2(walk), displacement.z);
        result = move_and_slide(level_.world, player_.body(), displacement, blockers);
    }
    player_.set_position(result.position);
    bool landed = vertical_speed < 0.0f && result.correction.z > 0.0f;

    // Walking down a slope, the ground drops away faster than gravity pulls the player down at first. If
    // there is ground close below, move down onto it, so the player doesn't fall in small hops.
    if (!landed && player_.on_ground() && vertical_speed <= 0.0f) {
        const float snap = glm::length(glm::vec2(displacement)) * max_snap_slope_;
        const SlideResult snapped = move_and_slide(level_.world, player_.body(), {0.0f, 0.0f, -snap}, blockers);
        if (snapped.correction.z > 0.0f) {
            player_.set_position(snapped.position);
            landed = true;
        }
    }

    // Landing on something, or bumping into something above, stops the vertical movement.
    const bool bumped = vertical_speed > 0.0f && result.correction.z < 0.0f;
    player_.set_vertical_speed(landed || bumped ? 0.0f : vertical_speed);
    player_.set_on_ground(landed);
    count_footsteps(glm::length(glm::vec2(player_.position() - start)),
                    landed && !was_on_ground ? -vertical_speed : 0.0f, running ? kRunStepLength : kWalkStepLength);
}

void Game::count_footsteps(float walked, float landing_speed, float step_length) {
    if (landing_speed > kLandingSpeed) {
        ++footsteps_;
        step_progress_ = 0.0f;
    }
    if (!player_.on_ground()) {
        return;
    }
    if (walked < kStandingDistance) {
        // Standing still: the next step comes soon after starting to walk again.
        step_progress_ = std::max(step_progress_, 1.0f - kFirstStepFraction);
        return;
    }
    step_progress_ += walked / step_length;
    while (step_progress_ >= 1.0f) {
        ++footsteps_;
        step_progress_ -= 1.0f;
    }
}

}  // namespace manifold
