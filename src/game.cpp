#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "physics/collision.hpp"
#include "physics/raycast.hpp"
#include "save/saved_game.hpp"
#include "util/log.hpp"
#include "world/chest.hpp"
#include "world/door.hpp"
#include "world/obstacle.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Running into something slows down to walking if the horizontal push out of the world geometry is longer than this
/// part of the frame's horizontal movement: sin 30 degrees. Moving at an angle a into a wall, the push is sin a of the
/// movement past the wall, so a run at less than 30 degrees to a wall slides along it at running speed. Ramps and
/// floors push straight up, so they don't slow running down.
constexpr float kRunBlockedSine = 0.5f;

/// The player at the start of `level`, turned the way it says, standing on the floor there, so that the first update
/// already walks rather than keeping the velocity of the air, see Game::update().
Player at_start(PlayerParams params, const Level& level) {
    params.position = level.player_start;
    params.yaw = level.player_yaw;
    Player player(params);
    player.set_on_ground(true);
    return player;
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

/// What the chests of `level` hold at its start, see Game::chest_inventories().
std::vector<Inventory> level_chest_inventories(const Level& level) {
    std::vector<Inventory> inventories;
    inventories.reserve(level.world.chests().size());
    for (const Chest& chest : level.world.chests()) {
        inventories.emplace_back(chest.items);
    }
    return inventories;
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
      party_config_(config.party),
      magic_config_(config.magic),
      seed_(seed),
      level_(std::move(level)),
      player_(at_start(config.player, level_)),
      party_(make_new_party(seed)),
      level_start_party_(party_),
      magic_(PartyMagic::fresh(config.magic)),
      level_start_magic_(magic_),
      chest_inventories_(level_chest_inventories(level_)),
      doors_(level_.world.doors().size()) {}

std::vector<Character> Game::make_new_party(std::uint64_t seed) const {
    return make_party(level_config_.assets, party_config_, seed ^ kPartySeedSalt);
}

void Game::set_hp(std::size_t index, int hp) {
    Character& character = party_.at(index);
    character.hp = std::clamp(hp, 0, character.max_hp);
}

void Game::teleport(const glm::vec3& position, std::optional<float> yaw) {
    player_.set_position(position);
    player_.set_vertical_speed(0.0f);
    player_.set_horizontal_velocity(glm::vec2(0.0f));
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
    magic_ = PartyMagic::fresh(magic_config_);
    inventory_ = Inventory{};
    start_level(std::move(level), seed, 0, layout);
}

namespace {

/// The items of `stacks` that `catalog` has, in order. Stacks of the same kind are joined, as Inventory::add() does.
/// Unknown keys are left out, after a warning.
///
/// \param owner Whose items they are, for the warning, e.g. "the party".
/// \throws std::overflow_error if the stacks of a kind have more items together than a stack can hold.
Inventory items_of_stacks(const ItemCatalog& catalog, const std::vector<SavedItemStack>& stacks, const char* owner) {
    Inventory inventory;
    for (const SavedItemStack& stack : stacks) {
        if (const std::optional<ItemId> item = catalog.find(stack.key)) {
            inventory.add(*item, stack.count);
        } else {
            log_warning("The saved item {} is unknown, so {} loses it", stack.key, owner);
        }
    }
    return inventory;
}

/// The stacks of `inventory` for a save, see SavedGame::inventory.
std::vector<SavedItemStack> saved_stacks(const ItemCatalog& catalog, const Inventory& inventory) {
    std::vector<SavedItemStack> stacks;
    stacks.reserve(inventory.size());
    for (const ItemStack& stack : inventory.stacks()) {
        stacks.push_back({.key = catalog.at(stack.item).key, .count = stack.count});
    }
    return stacks;
}

/// `magic` with its spells cut to their durations of `config`, which the data files may have shortened since a save was
/// made.
PartyMagic cut_spells(PartyMagic magic, const MagicConfig& config) {
    for (ActiveSpell& active : magic.spells) {
        active.remaining = std::min(active.remaining, config.spell(active.spell).duration);
    }
    return magic;
}

}  // namespace

void Game::load(const SavedGame& save) {
    Level level = generate_game_level(level_config_, save.layout, player_params_.radius, save.seed, save.level_number);
    if (save.progress) {
        const std::size_t doors = level.world.doors().size();
        const std::size_t chests = level.world.chests().size();
        if (save.progress->doors.size() != doors || save.progress->chests.size() != chests) {
            throw std::runtime_error(std::format(
                "the saved level has {} doors and {} chests, but the level generated from its seed has {} and {}",
                save.progress->doors.size(), save.progress->chests.size(), doors, chests));
        }
    }
    // The items first, which can fail, so that a failure changes nothing.
    Inventory start_inventory = items_of_stacks(level_config_.items, save.inventory, "the party");
    Inventory progress_inventory;
    std::vector<Inventory> chest_inventories;
    if (save.progress) {
        progress_inventory = items_of_stacks(level_config_.items, save.progress->inventory, "the party");
        for (const std::vector<SavedItemStack>& chest : save.progress->chests) {
            chest_inventories.push_back(items_of_stacks(level_config_.items, chest, "a chest"));
        }
    }
    time_ = save.time;
    party_ = save.party;
    magic_ = cut_spells(save.magic, magic_config_);
    inventory_ = std::move(start_inventory);
    start_level(std::move(level), save.seed, save.level_number, save.layout);
    if (!save.progress) {
        return;
    }
    const LevelProgress& progress = *save.progress;
    time_ = progress.time;
    party_ = progress.party;
    magic_ = cut_spells(progress.magic, magic_config_);
    inventory_ = std::move(progress_inventory);
    chest_inventories_ = std::move(chest_inventories);
    doors_ = progress.doors;
    player_.set_position(progress.player.position);
    player_.set_view(progress.player.yaw, progress.player.pitch);
    player_.set_vertical_speed(progress.player.vertical_speed);
    player_.set_horizontal_velocity(progress.player.horizontal_velocity);
    player_.set_on_ground(progress.player.on_ground);
}

SavedGame Game::level_start_save() const {
    return {.seed = seed_,
            .level_number = level_number_,
            .layout = given_layout_,
            .time = level_start_time_,
            .party = level_start_party_,
            .magic = level_start_magic_,
            .inventory = saved_stacks(level_config_.items, level_start_inventory_)};
}

SavedGame Game::progress_save() const {
    SavedGame save = level_start_save();
    LevelProgress progress{.time = time_,
                           .party = party_,
                           .magic = magic_,
                           .inventory = saved_stacks(level_config_.items, inventory_),
                           .player = {.position = player_.position(),
                                      .yaw = player_.yaw(),
                                      .pitch = player_.pitch(),
                                      .vertical_speed = player_.vertical_speed(),
                                      .horizontal_velocity = player_.horizontal_velocity(),
                                      .on_ground = player_.on_ground()},
                           .doors = doors_,
                           .chests = {}};
    progress.chests.reserve(chest_inventories_.size());
    for (const Inventory& chest : chest_inventories_) {
        progress.chests.push_back(saved_stacks(level_config_.items, chest));
    }
    save.progress = std::move(progress);
    return save;
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
    level_start_inventory_ = inventory_;
    chest_inventories_ = level_chest_inventories(level_);
    player_ = at_start(player_params_, level_);
    doors_.assign(level_.world.doors().size(), DoorState{});
    door_moves_.clear();
    chest_activations_.clear();
    footsteps_ = 0;
    step_progress_ = 1.0f - kFirstStepFraction;
}

bool Game::cast(Spell spell) {
    if (!magic_.cast(spell, magic_config_)) {
        return false;
    }
    casts_.push_back(spell);
    return true;
}

float Game::animation_time() const { return static_cast<float>(std::fmod(time_, kAnimationPeriod)); }

void Game::update(const PlayerInput& input, float dt) {
    time_ += dt;
    magic_.advance(dt, magic_config_.mana_regeneration);
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
    if (input.center_click) {
        activate();
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
    if (hit->target == RayTarget::kChest) {
        chest_activations_.push_back(hit->chest);
        return Activation::kChest;
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
    player_.rotate((input.turn * turn_speed_ * dt) + input.mouse_rotation.x,
                   (input.look * look_speed_ * dt) + input.mouse_rotation.y);
    if (dt <= 0.0f) {
        // Nothing moves, and the player stays on the ground or in the air, with the same velocity.
        return;
    }

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
    if (noclip_ || levitating()) {
        // Levitation is like noclip, but stopped by what is in the way. Without gravity there is nothing to stand on,
        // so there are no footsteps and no sticking to ramps. When noclip is turned off or the spell ends in the air,
        // the fall starts from rest.
        displacement.z = input.rise * walk_speed_ * dt;
        player_.set_position(noclip_ ? player_.position() + displacement
                                     : move_and_slide(level_.world, player_.body(), displacement, blockers).position);
        player_.set_vertical_speed(0.0f);
        player_.set_horizontal_velocity(glm::vec2(0.0f));
        player_.set_on_ground(false);
        return;
    }
    // In the air, there is nothing to push off: the player keeps the horizontal velocity of the take-off, whatever the
    // keys, and turning only turns the view.
    const bool was_on_ground = player_.on_ground();
    if (!was_on_ground) {
        running = false;
        displacement = glm::vec3(player_.horizontal_velocity() * dt, 0.0f);
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
    SlideResult result = move_and_slide(level_.world, player_.body(), displacement, blockers);
    // Running into something, such as a wall or a door, at more than 30 degrees moves only at walking speed, also
    // along it. Ramps and floors push the player straight up, so running on them goes on.
    if (running && glm::length(glm::vec2(result.correction)) > kRunBlockedSine * glm::length(glm::vec2(displacement))) {
        running = false;
        displacement = glm::vec3(glm::vec2(walk), displacement.z);
        result = move_and_slide(level_.world, player_.body(), displacement, blockers);
    }
    player_.set_position(result.position);
    // The velocity follows the actual movement, so what stops the player, such as a wall, also stops the velocity.
    player_.set_horizontal_velocity(glm::vec2(result.position - start) / dt);
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
