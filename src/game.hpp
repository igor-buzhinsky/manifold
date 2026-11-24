#pragma once

#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "level/texture_kind.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "save/saved_game.hpp"
#include "world/door.hpp"
#include "world/obstacle.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for a new game.
struct GameConfig {
    LevelConfig level;    ///< Settings for level generation.
    PlayerParams player;  ///< The player at the start. The position and yaw are replaced by the level's start.
    float turn_speed = std::numbers::pi_v<float> / 2.0f;  ///< Radians per second: 90 degrees.
    float look_speed = std::numbers::pi_v<float> / 3.0f;  ///< Radians per second: 60 degrees.
    /// Metres per second: walking, stepping aside, rising and sinking with noclip or Levitate, and what running
    /// becomes when it runs into something. A brisk walk.
    float walk_speed = 2.5f;
    /// Forward speed while running, in metres per second, see PlayerInput::run. A fast run, at 3:42 minutes per km.
    float run_speed = 4.5f;
    /// Steepest slope, as height per horizontal distance, that the player sticks to when walking down.
    float max_snap_slope = 1.0f;
    /// How high a jump lifts the player's feet under Earth gravity, in metres. The take-off speed is the same
    /// in every level, so the jump is higher where gravity is weaker: jump_height * kEarthGravity / gravity.
    float jump_height = 0.5f;
    /// The portraits that the characters of a new party are chosen from, see make_party().
    std::vector<PortraitFile> portraits;
    PartyConfig party;  ///< How many characters a new party has, and their hit points.
};

/// What the player wants to do during a frame, independent of the input device.
///
/// Rates are in [-1, 1], where 1 means full speed.
struct PlayerInput {
    float turn = 0.0f;  ///< Positive turns left.
    float look = 0.0f;  ///< Positive looks up.
    /// How far mouse look turns the view in this frame, besides `turn` and `look`: the change of yaw and of pitch in
    /// radians, see Player::rotate(), which keeps the pitch within its limit. Unlike the rates, it doesn't depend on
    /// the frame time.
    glm::vec2 mouse_rotation{0.0f};
    float forward = 0.0f;  ///< Positive walks forward, negative backward.
    /// Whether the player runs: walking forward is at GameConfig::run_speed instead of GameConfig::walk_speed. Only
    /// the forward part of the movement is faster, not walking backward or stepping aside. It has no effect while
    /// Levitate is in effect, nor in the air, where the velocity of the take-off is kept, and in a frame where the
    /// faster movement would run into something at more than 30 degrees from along it, the player moves at walking
    /// speed instead.
    bool run = false;
    float strafe = 0.0f;  ///< Positive steps right, negative left.
    /// Positive jumps, or flies up with noclip or levitation. Negative flies down with noclip or levitation. A jump
    /// needs a new press: rise must have been <= 0 in the previous frame.
    float rise = 0.0f;
    /// Whether the activation key is held. A new press activates what the player looks at, see Game::activate().
    bool activate = false;
    /// If set, the player clicked on the view in this frame, to activate what is in this direction from the eyes, see
    /// Game::activate(). A unit vector. Unlike `activate`, each click activates, since it is a single event.
    std::optional<glm::vec3> click = std::nullopt;
    /// Whether the player clicked at the center of the view in this frame, as with mouse look, which activates as a new
    /// press of `activate` does: along the view after this frame's turn and movement. Like `click`, each activates.
    bool center_click = false;
    /// The spells the player cast in this frame, in order, see Game::cast(). Like a click, each is a single event.
    // Without the initializer, GCC warns about inputs that leave it out, such as `{.forward = 1.0f}`.
    std::vector<Spell> casts = {};  // NOLINT(readability-redundant-member-init)
};

/// What Game::activate() did.
enum class Activation : std::uint8_t {
    kNothing,  ///< The player doesn't look at anything that can be activated, within reach.
    kOpened,   ///< A door started to open.
    kClosed,   ///< A door started to close.
    kRefused,  ///< The player looks at a door that can't start moving now, see Game::set_door_open().
};

/// A door that started to open or to close, see Game::take_door_moves().
struct DoorMove {
    std::size_t door = 0;  ///< Index of the door in World::doors().
    bool open = false;     ///< True if it started to open, false if it started to close.
};

/// Owns the game state.
///
/// The world holds static level geometry, while the player (and later other dynamic entities) live next
/// to it, so systems such as physics or AI can work on both.
class Game {
public:
    /// Generates the level, places the player at its start, and makes a new party, see new_game().
    ///
    /// The first level of a game, here and in new_game() and load(), gets an extra torch on the wall side of the start
    /// room nearest to the start that has none, if there is one (see generate_level()). The debug layout has no rooms,
    /// so it gets none. The party has no light of its own until it casts Candlelight.
    ///
    /// \param config Game settings.
    /// \param seed   Seed for level generation and the party. The same seed gives the same level and party.
    /// \param layout The layout of this level. If not given, it is chosen at random, see LevelConfig. The
    ///               layouts of the next levels are always chosen at random.
    /// \throws std::runtime_error if the level can't be generated.
    Game(const GameConfig& config, std::uint64_t seed, std::optional<LayoutKind> layout = std::nullopt);

    /// Starts a game in a level that is already generated, e.g. one built by hand in a test.
    ///
    /// \param config Game settings. The level settings are used for the next levels, with layouts chosen at
    ///               random.
    /// \param level  The level. The player starts at its start.
    /// \param seed   The seed reported by seed(). The next level is generated from next_level_seed() of it. The party
    ///               is made from it, as by new_game().
    Game(const GameConfig& config, Level level, std::uint64_t seed = 0);

    /// Advances the game by one frame.
    ///
    /// Turns the player first, then walks in the new direction. Without noclip, the player falls by the
    /// level's gravity and is stopped by obstacles and level walls. Walking into a ramp goes up it, and a
    /// player who walks down a ramp sticks to it instead of falling in small hops. When `input.rise` becomes
    /// positive, a player who stands on something jumps. Holding it doesn't jump again. In the air, after a jump or a
    /// step off a ledge, the player keeps the horizontal velocity of the take-off whatever the keys, except for what
    /// obstacles stop. With noclip, the player flies:
    /// moves up and down by `input.rise`, doesn't fall, and goes through everything. While Levitate is in effect, the
    /// player flies the same way but is stopped by obstacles, walls and doors, makes no footsteps and doesn't stick to
    /// ramps; when it ends in the air, the player falls from rest, also horizontally, as after noclip.
    ///
    /// First of all, the party's magic advances, see PartyMagic::advance(), and then the spells of `input.casts` are
    /// cast, see cast(). So a spell cast in this frame lasts its whole duration from the end of the frame, and a
    /// Levitate cast in it already lets the player fly.
    ///
    /// Then the doors move. Doors that are not passable, see DoorState::passable(), block the player like obstacles.
    /// When `input.activate` becomes true, the player activates what they look at, and then, if `input.click` is set,
    /// what is in its direction, see activate(). Both happen after the player moved. The click's direction is not
    /// turned with the player, so it stays that of the picture that was clicked.
    ///
    /// If the player then touches the level exit, also with noclip, the next level starts, see next_level().
    ///
    /// \param input What the player wants to do.
    /// \param dt    Duration of the frame in seconds.
    /// \throws std::runtime_error if the next level can't be generated.
    void update(const PlayerInput& input, float dt);

    /// Generates the next level from next_level_seed() of the current seed, and puts the player at its start.
    ///
    /// The layout is chosen at random, see LevelConfig, even if the first level was given one.
    ///
    /// The player stands still and looks straight ahead. Noclip, the game time, the party and its magic (mana and the
    /// spells in effect) stay as they are.
    ///
    /// \throws std::runtime_error if the level can't be generated. The current level then stays.
    void next_level();

    /// Starts a new game: generates its first level from `seed`, as the constructor does, and puts the player at its
    /// start.
    ///
    /// The level number and the game time start again from 0. Noclip stays as it is. A new party is made with
    /// make_party(), from GameConfig::portraits and a seed derived from `seed`, so the same seed gives the same party.
    /// It has full mana and no spells in effect.
    ///
    /// \param seed   Seed for level generation and the party.
    /// \param layout The layout of the first level, or nothing to choose it at random.
    /// \throws std::runtime_error if the level can't be generated. The current level then stays.
    void new_game(std::uint64_t seed, std::optional<LayoutKind> layout);

    /// Starts the saved level again from its start, as next_level() starts a level.
    ///
    /// The level is generated from the saved seed, with the saved layout if one was given, and the level number and
    /// the game time, the party and its magic become the saved ones. Noclip stays as it is.
    ///
    /// \throws std::runtime_error if the level can't be generated. The current level then stays.
    void load(const SavedGame& save);

    /// What a save of this level holds, to start it again with load(): the level, the party and its magic as they
    /// were at its start.
    ///
    /// A level that was not generated, but given to the constructor, is saved with its seed, so loading it gives the
    /// level generated from that seed instead.
    SavedGame save_point() const;

    /// Activates what the center of the player's view points at, or what is in another direction from the eyes, within
    /// kActivationReach of the eyes.
    ///
    /// Aiming at a door that is not passable opens it, and aiming at a frame of a door opens or closes it, see
    /// set_door_open(). Walls, slabs and doors in front stop the view.
    ///
    /// \param direction Unit vector from the eyes, e.g. through the point of the view that the player clicked, or none
    ///                  for the center of the view, see Player::view_direction(). The reach is the same either way.
    Activation activate(std::optional<glm::vec3> direction = std::nullopt);

    /// Starts to open or close a door. A door that starts to move is added to take_door_moves().
    ///
    /// \param index Index of the door in World::doors().
    /// \param open  True to open it, false to close it.
    /// \return False if the door doesn't start to move: it is moving, it is already open or closed as wanted, or it
    ///         should close while the player overlaps it, see Door::blocker().
    /// \throws std::out_of_range if there is no such door.
    bool set_door_open(std::size_t index, bool open);

    /// Sets the hit points of a character of the party, e.g. as a cheat. They are clamped to [0, Character::max_hp].
    ///
    /// \param index Index of the character in party().
    /// \param hp    The new hit points.
    /// \throws std::out_of_range if there is no such character.
    void set_hp(std::size_t index, int hp);

    /// Casts a spell for the party, if it has enough mana, see PartyMagic::cast().
    ///
    /// \return False, with nothing changed, if the party has less mana than the spell costs.
    bool cast(Spell spell) { return magic_.cast(spell); }

    /// Sets the party's mana, e.g. as a cheat. It is clamped to [0, PartyMagic::max_mana].
    void set_mana(float mana) { magic_.set_mana(mana); }

    /// Sets what the surfaces of the level are made of, see Level::texture_kind. The next level is stone until this
    /// is called again.
    void set_texture_kind(TextureKind kind) { level_.texture_kind = kind; }

    /// How many footsteps the player made since the last call, e.g. to play their sounds. Resets the count.
    ///
    /// Without noclip, a player on the ground makes a step every kWalkStepLength walked, or kRunStepLength run, the
    /// first one a quarter of a step after starting to move from standing. Landing at more than kLandingSpeed, e.g.
    /// after a jump, is a step too. The count starts again from 0 in each level and after teleport().
    int take_footsteps() { return std::exchange(footsteps_, 0); }

    /// The doors that started to move since the last call, in the order they started, e.g. to play their sounds.
    /// Clears the list. The moves of a level are dropped when the next level starts.
    std::vector<DoorMove> take_door_moves();

    /// Moves the player, e.g. to test a level, without checking for obstacles.
    ///
    /// The player stops falling, and falls again from rest at the next update unless noclip is on.
    ///
    /// \param position The point between the player's feet.
    /// \param yaw      Direction to face, in radians counter-clockwise from +x, or none to keep it.
    void teleport(const glm::vec3& position, std::optional<float> yaw = std::nullopt);

    /// Whether noclip is on: the player flies through obstacles and walls.
    bool noclip() const { return noclip_; }
    /// Turns noclip on or off.
    void set_noclip(bool noclip) { noclip_ = noclip; }

    const Level& level() const { return level_; }
    const World& world() const { return level_.world; }
    const Player& player() const { return player_; }
    /// The characters of the party, in the order of their portraits from the left.
    std::span<const Character> party() const { return party_; }
    /// The party's shared mana, and the spells in effect.
    const PartyMagic& magic() const { return magic_; }
    /// Whether Levitate is in effect, so that the player flies, see update().
    bool levitating() const { return magic_.active(Spell::kLevitate); }
    /// The state of each door of World::doors(), in the same order.
    std::span<const DoorState> doors() const { return doors_; }
    /// The seed the level was generated from.
    std::uint64_t seed() const { return seed_; }
    /// How many levels came before this one: 0 in the first level.
    int level_number() const { return level_number_; }
    /// How many times a level was started after the first one, by next_level(), load() or new_game(), e.g. to tell when
    /// to make a new renderer. Unlike level_number(), it changes when a game is loaded at the same level.
    int level_starts() const { return level_starts_; }
    /// Seconds of game time since the start: the sum of the durations passed to update().
    double time() const { return time_; }
    /// Game time for animations, such as the flicker of torches: time() wrapped into [0, kAnimationPeriod).
    ///
    /// Floats lose precision as they grow, which makes fast animations stutter after long play. Wrapping
    /// keeps them smooth, at the cost of one jump per period.
    float animation_time() const;

    /// Length of the cycle of animation_time(), in seconds: one hour.
    static constexpr double kAnimationPeriod = 3600.0;
    /// How far the player walks per footstep, in metres. At the default walk_speed of 2.5 m/s, that is 2.5 steps per
    /// second.
    static constexpr float kWalkStepLength = 1.0f;
    /// How far the player runs per footstep, in metres. At the default run_speed of 4.5 m/s, that is 3 steps per
    /// second, a usual cadence of running.
    static constexpr float kRunStepLength = 1.5f;
    /// The slowest vertical speed of landing that makes a footstep, in metres per second. A jump of 0.5 m lands at
    /// about 3 m/s, and walking down a ramp lands much slower.
    static constexpr float kLandingSpeed = 2.0f;
    /// The part of a step that the player moves from standing before the first footstep.
    static constexpr float kFirstStepFraction = 0.25f;
    /// Moving less than this in a frame, in metres, counts as standing still for footsteps, e.g. pushing into a wall.
    static constexpr float kStandingDistance = 1e-4f;
    /// How far from the eyes the player can activate things, in metres.
    static constexpr float kActivationReach = 2.0f;

private:
    /// Turns and moves the player, see update().
    ///
    /// \param blockers Obstacles that are not part of the world, see door_blockers().
    void move_player(const PlayerInput& input, float dt, std::span<const Obstacle> blockers);

    /// Puts the player at the start of a new level, which replaces the current one, see next_level().
    ///
    /// \param level        The new level.
    /// \param seed         The seed it was generated from.
    /// \param level_number How many levels came before it.
    /// \param layout       The layout it was generated with, if it was given rather than chosen from the seed.
    void start_level(Level level, std::uint64_t seed, int level_number, std::optional<LayoutKind> layout);

    /// Counts the footsteps of a frame, see take_footsteps(), after the player moved and on_ground() was updated.
    ///
    /// \param walked        How far the player moved horizontally in the frame, in metres.
    /// \param landing_speed The vertical speed at which the player landed in the frame, positive downwards, or 0.
    /// \param step_length   The length of a step of the frame's gait, kWalkStepLength or kRunStepLength, in metres.
    void count_footsteps(float walked, float landing_speed, float step_length);

    /// The blockers of the doors that are not passable, see Door::blocker().
    std::vector<Obstacle> door_blockers() const;

    /// A new party for a game whose first level has the seed `seed`, see new_game().
    std::vector<Character> make_new_party(std::uint64_t seed) const;

    LevelConfig level_config_;
    PlayerParams player_params_;
    float turn_speed_;
    float look_speed_;
    float walk_speed_;
    float run_speed_;
    float max_snap_slope_;
    float jump_speed_;                     ///< Take-off speed of a jump, in metres per second.
    std::vector<PortraitFile> portraits_;  ///< See GameConfig::portraits.
    PartyConfig party_config_;
    bool noclip_ = false;
    bool rise_held_ = false;      ///< Whether input.rise was positive in the previous frame.
    bool activate_held_ = false;  ///< Whether input.activate was true in the previous frame.
    double time_ = 0.0;
    double level_start_time_ = 0.0;  ///< The game time when the level started.
    std::uint64_t seed_;
    std::optional<LayoutKind> given_layout_;  ///< The layout of the level, if it was given, see SavedGame::layout.
    int level_number_ = 0;
    int level_starts_ = 0;  ///< See level_starts().
    Level level_;
    Player player_;
    std::vector<Character> party_;
    std::vector<Character> level_start_party_;  ///< The party when the level started, see save_point().
    PartyMagic magic_;                          ///< See magic().
    PartyMagic level_start_magic_;              ///< The party's magic when the level started, see save_point().
    std::vector<DoorState> doors_;              ///< The state of each door of the level.
    std::vector<DoorMove> door_moves_;          ///< See take_door_moves().
    int footsteps_ = 0;                         ///< See take_footsteps().
    /// How much of a step the player has moved on the ground since the last footstep, from 0 to 1. Running and
    /// walking add to it by their own step lengths.
    float step_progress_ = 1.0f - kFirstStepFraction;
};

}  // namespace manifold
