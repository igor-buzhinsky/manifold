#include "console/game_commands.hpp"

#include <cstddef>
#include <format>
#include <functional>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "console/console.hpp"
#include "game.hpp"
#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "save/saved_game.hpp"
#include "util/asset_mapping.hpp"
#include "util/log.hpp"
#include "world/chest.hpp"
#include "world/door.hpp"
#include "world/player.hpp"

namespace manifold {

namespace {

float to_degrees(float radians) { return radians * 180.0f / std::numbers::pi_v<float>; }
float to_radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0f; }

/// Throws CommandError with the usage unless there are `count` arguments.
void expect_count(std::span<const std::string> arguments, std::size_t count, const std::string& usage) {
    if (arguments.size() != count) {
        throw CommandError("usage: " + usage);
    }
}

/// Adds a command that turns a setting on or off, or toggles it when run without an argument, and logs the
/// new state.
///
/// \param console Where to add the command.
/// \param name    The command, e.g. `shadows`.
/// \param label   The setting in the log message, e.g. `Shadows`.
/// \param help    What the command does.
/// \param get     Tells whether the setting is on.
/// \param set     Turns the setting on or off.
void add_switch(Console& console, const std::string& name, const std::string& label, const std::string& help,
                std::function<bool()> get, std::function<void(bool)> set) {
    const std::string usage = name + " [on|off]";
    console.add(name, usage, help + " Without on or off, toggles it.",
                [get = std::move(get), set = std::move(set), usage, label](std::span<const std::string> arguments) {
                    if (arguments.size() > 1) {
                        throw CommandError("usage: " + usage);
                    }
                    set(arguments.empty() ? !get() : parse_switch_argument(arguments[0]));
                    log_info("{} {}", label, get() ? "on" : "off");
                });
}

/// Adds a command that turns `setting` on or off, see the other add_switch().
void add_switch(Console& console, const std::string& name, const std::string& label, const std::string& help,
                bool& setting) {
    add_switch(console, name, label, help, [&setting] { return setting; }, [&setting](bool on) { setting = on; });
}

/// Parses a screenshot size in pixels.
int parse_screenshot_size(const std::string& text) {
    const int size = parse_int_argument(text);
    if (size < 1 || size > kMaxScreenshotSize) {
        throw CommandError(std::format("screenshot size must be from 1 to {}: {}", kMaxScreenshotSize, text));
    }
    return size;
}

/// Adds the commands that move the player forward for a number of frames: walk and run.
void add_walk_commands(Console& console, ConsoleState& state) {
    // `walk` and `run` differ only in whether the player runs.
    for (const bool runs : {false, true}) {
        const std::string name = runs ? "run" : "walk";
        const std::string usage = name + " FRAMES";
        console.add(name, usage,
                    runs ? "Runs the player forward for this many frames, as holding W and Shift does."
                         : "Walks the player forward for this many frames, as holding W does.",
                    [&state, usage, runs](std::span<const std::string> arguments) {
                        expect_count(arguments, 1, usage);
                        const int frames = parse_int_argument(arguments[0]);
                        if (frames < 0) {
                            throw CommandError("frames must not be negative: " + arguments[0]);
                        }
                        state.walk_frames = frames;
                        state.walk_runs = runs;
                    });
    }
}

/// Describes the player's state for the player_info command: where they are and look, how they move, and noclip and
/// Levitate.
std::string describe_player(const Game& game) {
    const Player& player = game.player();
    const glm::vec2& velocity = player.horizontal_velocity();
    return std::format(
        "Player at ({:.2f}, {:.2f}, {:.2f}), yaw {:.1f} deg, pitch {:.1f} deg\n"
        "{}, horizontal velocity ({:.2f}, {:.2f}) m/s, {:.2f} m/s, vertical speed {:.2f} m/s\n"
        "Noclip {}, Levitate {}",
        player.position().x, player.position().y, player.position().z, to_degrees(player.yaw()),
        to_degrees(player.pitch()), player.on_ground() ? "On the ground" : "In the air", velocity.x, velocity.y,
        glm::length(velocity), player.vertical_speed(), game.noclip() ? "on" : "off",
        game.levitating() ? "in effect" : "not in effect");
}

/// Adds the commands that inspect and move the player and the camera: level_info, player_info, camera, teleport,
/// walk, run, next_level, load and noclip.
void add_player_commands(Console& console, CommandQueue& queue, Game& game, ConsoleState& state) {
    add_walk_commands(console, state);
    console.add(
        "level_info", "level_info", "Describes the level, depending on its layout, its texture and its ambient light.",
        [&game, &state](std::span<const std::string> arguments) {
            expect_count(arguments, 0, "level_info");
            const glm::vec3& ambient = state.view.level_ambient;
            const LevelTexture& texture = game.level().texture;
            log_info("Level seed {}, layout {}:\n{}Texture {} {}\nAmbient light ({:.3f}, {:.3f}, {:.3f})", game.seed(),
                     layout_name(game.level().layout), game.level().info,
                     asset_key_name(level_textures_key(texture.kind)), texture.index, ambient.r, ambient.g, ambient.b);
        });
    console.add("player_info", "player_info",
                "Describes the player: position, view, velocity, whether on the ground, noclip and Levitate.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "player_info");
                    log_info("{}", describe_player(game));
                });

    const std::string camera_usage = "camera [X Y Z YAW PITCH | player]";
    console.add("camera", camera_usage,
                "Shows where the camera is, puts it in place (angles in degrees), or back at the player's eyes.",
                [&game, &state, camera_usage](std::span<const std::string> arguments) {
                    if (arguments.empty()) {
                        const Camera camera = view_camera(state.view, game.player());
                        log_info("Camera {}at ({:.2f}, {:.2f}, {:.2f}), yaw {:.1f} deg, pitch {:.1f} deg",
                                 state.view.camera ? "" : "of the player ", camera.position.x, camera.position.y,
                                 camera.position.z, to_degrees(camera.yaw), to_degrees(camera.pitch));
                    } else if (arguments.size() == 1 && arguments[0] == "player") {
                        state.view.camera.reset();
                    } else {
                        expect_count(arguments, 5, camera_usage);
                        const float pitch = parse_float_argument(arguments[4]);
                        if (pitch < -90.0f || pitch > 90.0f) {
                            throw CommandError("pitch must be from -90 to 90 degrees: " + arguments[4]);
                        }
                        state.view.camera =
                            Camera{.position = {parse_float_argument(arguments[0]), parse_float_argument(arguments[1]),
                                                parse_float_argument(arguments[2])},
                                   .yaw = to_radians(parse_float_argument(arguments[3])),
                                   .pitch = to_radians(pitch)};
                    }
                });

    const std::string teleport_usage = "teleport X Y Z [YAW]";
    console.add("teleport", teleport_usage,
                "Moves the player's feet to (X, Y, Z), without checking for obstacles, facing YAW degrees if given.",
                [&game, teleport_usage](std::span<const std::string> arguments) {
                    if (arguments.size() != 3) {
                        expect_count(arguments, 4, teleport_usage);
                    }
                    const glm::vec3 position{parse_float_argument(arguments[0]), parse_float_argument(arguments[1]),
                                             parse_float_argument(arguments[2])};
                    std::optional<float> yaw;
                    if (arguments.size() == 4) {
                        yaw = to_radians(parse_float_argument(arguments[3]));
                    }
                    game.teleport(position, yaw);
                });

    console.add("next_level", "next_level", "Starts the next level, as if the player reached the exit.",
                [&game, &queue](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "next_level");
                    try {
                        game.next_level();
                    } catch (const std::runtime_error& error) {
                        throw CommandError(error.what());
                    }
                    // The main loop sets up the new level, such as its ambient light, after the commands of this
                    // frame, so the next commands wait for it.
                    queue.wait(1);
                });

    const std::string load_usage = "load [level_start]";
    console.add(
        "load", load_usage,
        "Loads the saved game as it was at the last autosave or exit, as F9 does, or with level_start, from the "
        "start of its level.",
        [&queue, &state, load_usage](std::span<const std::string> arguments) {
            if (arguments.size() > 1 || (arguments.size() == 1 && arguments[0] != "level_start")) {
                throw CommandError("usage: " + load_usage);
            }
            state.load = arguments.empty() ? SaveKind::kProgress : SaveKind::kLevelStart;
            state.load_stops_on_error = queue.running_stops_on_error();
            // The main loop loads at the start of the next frame, before the next commands run.
            queue.wait(1);
        });

    add_switch(
        console, "noclip", "Noclip", "Lets the player fly through everything (Page Up / Page Down), or not.",
        [&game] { return game.noclip(); }, [&game](bool on) { game.set_noclip(on); });
}

/// What a door is doing, for the log: open, closed, opening or closing.
const char* door_state_name(const DoorState& door) {
    if (door.moving()) {
        return door.target > door.openness ? "opening" : "closing";
    }
    return door.openness >= 1.0f ? "open" : "closed";
}

/// Adds the commands that inspect and use doors: doors and door.
void add_door_commands(Console& console, Game& game) {
    console.add("doors", "doors",
                "Lists the doors: what they are made of, their number, where they are, and whether they are open.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "doors");
                    const std::span<const Door> doors = game.world().doors();
                    std::string list;
                    for (std::size_t i = 0; i < doors.size(); ++i) {
                        const glm::vec3& center = doors[i].base_center;
                        list +=
                            std::format("\nDoor {} at ({:.2f}, {:.2f}, {:.2f}), wall along {:.0f} deg: {}", i, center.x,
                                        center.y, center.z, to_degrees(doors[i].yaw), door_state_name(game.doors()[i]));
                    }
                    if (doors.empty()) {
                        log_info("No doors");
                    } else {
                        log_info("{} doors of {}{}", doors.size(), door_look_name(game.world().door_look()), list);
                    }
                });

    const std::string door_usage = "door N open|close";
    console.add("door", door_usage, "Starts to open or close door N, see doors.",
                [&game, door_usage](std::span<const std::string> arguments) {
                    expect_count(arguments, 2, door_usage);
                    const int index = parse_int_argument(arguments[0]);
                    if (index < 0 || static_cast<std::size_t>(index) >= game.world().doors().size()) {
                        throw CommandError("no such door: " + arguments[0]);
                    }
                    if (arguments[1] != "open" && arguments[1] != "close") {
                        throw CommandError("usage: " + door_usage);
                    }
                    const auto door = static_cast<std::size_t>(index);
                    const bool open = arguments[1] == "open";
                    if (!game.set_door_open(door, open)) {
                        const DoorState& state = game.doors()[door];
                        // A door that rests in the other state than asked for only fails to close on the player.
                        const bool player_inside = !state.moving() && (state.target >= 1.0f) != open;
                        throw CommandError(std::format("door {} can't {} now: {}", index, arguments[1],
                                                       player_inside ? std::string("the player stands in it")
                                                                     : "it is " + std::string(door_state_name(state))));
                    }
                });
}

/// The party's mana and the spells in effect, for the log, e.g. `mana 95.0/100, spells: Candlelight 59.5 s`.
std::string describe_magic(const PartyMagic& magic, const MagicConfig& config) {
    std::string text = std::format("mana {:.1f}/{:.0f}, spells:", magic.mana, magic.max_mana);
    if (magic.spells.empty()) {
        return text + " none";
    }
    for (std::size_t i = 0; i < magic.spells.size(); ++i) {
        text += std::format("{} {} {:.1f} s", i == 0 ? "" : ",", config.spell(magic.spells[i].spell).name,
                            magic.spells[i].remaining);
    }
    return text;
}

/// The names of all spells, for messages: `candlelight|levitate`.
std::string spell_names() {
    std::string names;
    for (const Spell spell : kAllSpells) {
        names += std::string(names.empty() ? "" : "|") + spell_key(spell);
    }
    return names;
}

/// Adds the commands that inspect and change the party and its magic: party, hp, cast and mana.
void add_party_commands(Console& console, Game& game) {
    console.add("party", "party", "Lists the characters of the party: their gender, hit points and portrait.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "party");
                    const std::span<const Character> party = game.party();
                    std::string list;
                    for (std::size_t i = 0; i < party.size(); ++i) {
                        const Character& character = party[i];
                        const std::optional<Portrait>& portrait = character.portrait;
                        list += std::format(
                            "\nCharacter {}: {}, {}/{} HP, portrait {}", i, gender_name(character.gender), character.hp,
                            character.max_hp,
                            portrait ? std::format("{} {}", asset_key_name(portrait->key), portrait->index) : "none");
                    }
                    log_info("Party of {}, {}{}", party.size(), describe_magic(game.magic(), game.magic_config()),
                             list);
                });

    const std::string hp_usage = "hp N VALUE";
    console.add("hp", hp_usage, "Sets the hit points of character N, see party, from 0 to their maximum (a cheat).",
                [&game, hp_usage](std::span<const std::string> arguments) {
                    expect_count(arguments, 2, hp_usage);
                    const int index = parse_int_argument(arguments[0]);
                    if (index < 0 || static_cast<std::size_t>(index) >= game.party().size()) {
                        throw CommandError("no such character: " + arguments[0]);
                    }
                    const auto character = static_cast<std::size_t>(index);
                    const int max_hp = game.party()[character].max_hp;
                    const int hp = parse_int_argument(arguments[1]);
                    if (hp < 0 || hp > max_hp) {
                        throw CommandError(std::format("hit points must be from 0 to {}: {}", max_hp, arguments[1]));
                    }
                    game.set_hp(character, hp);
                });

    const std::string cast_usage = "cast " + spell_names();
    console.add("cast", cast_usage, "Casts a spell, as its key does, if the party has enough mana for it.",
                [&game, cast_usage](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, cast_usage);
                    const std::optional<Spell> spell = parse_spell_key(arguments[0]);
                    if (!spell) {
                        throw CommandError("no such spell: " + arguments[0] + ", expected " + spell_names());
                    }
                    if (!game.cast(*spell)) {
                        const SpellType& type = game.magic_config().spell(*spell);
                        throw CommandError(std::format("not enough mana for {}: it costs {:g}, the party has {:.1f}",
                                                       type.name, type.cost, game.magic().mana));
                    }
                    log_info("Cast {}: {}", game.magic_config().spell(*spell).name,
                             describe_magic(game.magic(), game.magic_config()));
                });

    const std::string mana_usage = "mana VALUE";
    console.add("mana", mana_usage, "Sets the party's mana, from 0 to its maximum (a cheat).",
                [&game, mana_usage](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, mana_usage);
                    const float max_mana = game.magic().max_mana;
                    const float mana = parse_float_argument(arguments[0]);
                    if (mana < 0.0f || mana > max_mana) {
                        throw CommandError(std::format("mana must be from 0 to {:.0f}: {}", max_mana, arguments[0]));
                    }
                    game.set_mana(mana);
                });
}

/// Logs what an activation did.
void log_activation(Activation activation) {
    switch (activation) {
        case Activation::kNothing:
            log_info("Nothing to activate");
            break;
        case Activation::kOpened:
            log_info("A door opens");
            break;
        case Activation::kClosed:
            log_info("A door closes");
            break;
        case Activation::kRefused:
            log_info("The door can't move now");
            break;
        case Activation::kChest:
            log_info("A chest is activated");
            break;
    }
}

/// Adds the activate command.
void add_activate_command(Console& console, Game& game) {
    const std::string activate_usage = "activate [X Y]";
    console.add("activate", activate_usage,
                "Activates what the center of the player's view points at, like the E key, or what a click at X Y "
                "would: fractions of the view's width and height from its top-left corner.",
                [&game, activate_usage](std::span<const std::string> arguments) {
                    if (!arguments.empty() && arguments.size() != 2) {
                        throw CommandError("usage: " + activate_usage);
                    }
                    std::optional<glm::vec3> direction;
                    if (arguments.size() == 2) {
                        const glm::vec2 point{parse_float_argument(arguments[0]), parse_float_argument(arguments[1])};
                        if (point.x < 0.0f || point.x > 1.0f || point.y < 0.0f || point.y > 1.0f) {
                            throw CommandError("X and Y must be from 0 to 1");
                        }
                        direction = make_player_camera(game.player()).ray_direction(point);
                    }
                    log_activation(game.activate(direction));
                });
}

/// Adds the commands that change how the game is shown, take screenshots and control the script: cutaway,
/// arrow, crosshair, lighting, shadows, imgui, screenshot, menu, wait and quit.
void add_view_commands(Console& console, CommandQueue& queue, ConsoleState& state) {
    console.add("cutaway", "cutaway HEIGHT|off",
                "Leaves out everything above HEIGHT, to look into a layer from above, or shows everything again.",
                [&state](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, "cutaway HEIGHT|off");
                    if (arguments[0] == "off") {
                        state.view.cutaway.reset();
                    } else {
                        state.view.cutaway = parse_float_argument(arguments[0]);
                    }
                });

    add_switch(console, "arrow", "Arrow",
               "Shows an arrow at the player's feet, in the direction the player faces, on top of everything, or not.",
               state.view.arrow);
    add_switch(console, "crosshair", "Crosshair",
               "Shows a crosshair at the center of the player's view, or not. It is hidden while the camera is put in "
               "place with camera.",
               state.view.crosshair);
    add_switch(console, "lighting", "Lighting", "Turns lights on, or off to show the plain texture colors.",
               state.view.lighting);
    add_switch(console, "shadows", "Shadows", "Turns shadows on or off.", state.view.shadows);
    add_switch(console, "vsync", "Vsync",
               "Lets showing a frame wait for the screen, as it does at first, or not, e.g. to profile without the "
               "limit of the screen's refresh rate.",
               state.vsync);
    console.add("profile", "profile FRAMES",
                "Measures how long the phases of the next FRAMES frames take and logs a summary. The next commands "
                "run once it is done.",
                [&queue, &state](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, "profile FRAMES");
                    const int frames = parse_int_argument(arguments[0]);
                    if (frames < 1) {
                        throw CommandError("frames must be positive: " + arguments[0]);
                    }
                    state.profile = frames;
                    // The profile measures this frame and the next ones, and logs its summary at the end of the last.
                    queue.wait(frames);
                });
    add_switch(console, "imgui", "Debug overlay",
               "Shows or hides the debug overlay, which tunes the lights and shows how the game runs.",
               state.debug_overlay);

    const std::string screenshot_usage = "screenshot FILE [WIDTH HEIGHT]";
    console.add("screenshot", screenshot_usage,
                "Saves the view as a PNG file after this frame, by default as large as the window. The next commands "
                "run in the next frame.",
                [&queue, &state, screenshot_usage](std::span<const std::string> arguments) {
                    if (arguments.size() != 1) {
                        expect_count(arguments, 3, screenshot_usage);
                    }
                    ScreenshotRequest request{.path = arguments[0],
                                              .size = std::nullopt,
                                              .view = state.view,
                                              .stop_on_error = queue.running_stops_on_error()};
                    if (arguments.size() == 3) {
                        request.size = PixelSize{.width = parse_screenshot_size(arguments[1]),
                                                 .height = parse_screenshot_size(arguments[2])};
                    }
                    state.screenshots.push_back(std::move(request));
                    // The picture is drawn after the commands of this frame, so commands after it that move the player
                    // or change the game would show in it. They wait for the next frame instead.
                    queue.wait(1);
                });

    const std::string menu_usage = "menu start|game|close";
    console.add("menu", menu_usage,
                "Shows the start menu or the game menu instead of the open menus, or closes them. The game is paused "
                "while a menu is open.",
                [&queue, &state, menu_usage](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, menu_usage);
                    if (arguments[0] == "start") {
                        state.menu = MenuRequest::kStart;
                    } else if (arguments[0] == "game") {
                        state.menu = MenuRequest::kGame;
                    } else if (arguments[0] == "close") {
                        state.menu = MenuRequest::kClose;
                    } else {
                        throw CommandError("usage: " + menu_usage);
                    }
                    // The main loop opens the menu at the start of the next frame, so that a screenshot after it
                    // shows the menu.
                    queue.wait(1);
                });

    console.add("wait", "wait FRAMES", "Runs the next commands after this many frames.",
                [&queue](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, "wait FRAMES");
                    const int frames = parse_int_argument(arguments[0]);
                    if (frames < 0) {
                        throw CommandError("frames must not be negative: " + arguments[0]);
                    }
                    queue.wait(frames);
                });

    console.add("quit", "quit", "Quits the game after this frame, without running the commands after it.",
                [&queue, &state](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "quit");
                    state.quit = true;
                    if (const std::size_t dropped = queue.clear(); dropped > 0) {
                        log_warning("Quitting without running the {} command(s) after quit", dropped);
                    }
                });
}

/// Describes the stacks of an inventory for the console, one per line, each after a line break: its index, number of
/// items, key, name, and the weight and value of one item, see ItemType.
std::string describe_items(const Inventory& inventory, const ItemCatalog& catalog) {
    std::string list;
    for (std::size_t i = 0; i < inventory.size(); ++i) {
        const ItemStack& stack = inventory.stacks()[i];
        const ItemType& item = catalog.at(stack.item);
        list += std::format("\n{}: {} x {} ({}), weight {}, value {}", i, stack.count, item.key, item.name,
                            format_weight(item.weight), item.value);
    }
    return list;
}

/// Parses the number of a chest of the level.
///
/// \throws CommandError if there is no such chest.
std::size_t parse_chest(const std::string& text, const Game& game) {
    const int index = parse_int_argument(text);
    if (index < 0 || static_cast<std::size_t>(index) >= game.world().chests().size()) {
        throw CommandError(std::format("no such chest: {}, the level has {}", text, game.world().chests().size()));
    }
    return static_cast<std::size_t>(index);
}

/// The most items that one `give` gives, which keeps stacks far from the most that an int counts.
constexpr int kMaxGiveCount = 1000000;

/// "1 item" or "N items".
std::string item_count(long long count) { return std::format("{} item{}", count, count == 1 ? "" : "s"); }

/// "1 stack" or "N stacks".
std::string stack_count(std::size_t count) { return std::format("{} stack{}", count, count == 1 ? "" : "s"); }

/// Adds the commands that inspect and change items: items, inventory, give, chests and open.
void add_item_commands(Console& console, CommandQueue& queue, Game& game, ConsoleState& state) {
    console.add("items", "items", "Lists all kinds of items: their keys, names, weights, values and chances.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "items");
                    std::string list;
                    for (const ItemType& item : game.items().items()) {
                        list += std::format("\n{} ({}), weight {}, value {}, chance {}", item.key, item.name,
                                            format_weight(item.weight), item.value, item.chance);
                    }
                    log_info("{} kinds of items{}", game.items().items().size(), list);
                });
    console.add("inventory", "inventory", "Lists the party's items, with their total weight and value.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "inventory");
                    const Inventory& inventory = game.inventory();
                    log_info("The party carries {} in {}, weight {}, value {}{}", item_count(inventory.item_count()),
                             stack_count(inventory.size()), format_weight(inventory.weight(game.items())),
                             inventory.value(game.items()), describe_items(inventory, game.items()));
                });

    const std::string give_usage = "give ITEM [COUNT]";
    console.add("give", give_usage, "Gives the party COUNT items of the key ITEM, see items, by default one (a cheat).",
                [&game, give_usage](std::span<const std::string> arguments) {
                    if (arguments.size() != 1) {
                        expect_count(arguments, 2, give_usage);
                    }
                    const std::optional<ItemId> item = game.items().find(arguments[0]);
                    if (!item) {
                        throw CommandError("no such item: " + arguments[0]);
                    }
                    const int count = arguments.size() == 2 ? parse_int_argument(arguments[1]) : 1;
                    if (count < 1 || count > kMaxGiveCount) {
                        throw CommandError(std::format("count must be from 1 to {}: {}", kMaxGiveCount, arguments[1]));
                    }
                    try {
                        game.inventory().add(*item, count);
                    } catch (const std::overflow_error&) {
                        throw CommandError("the party's stack of that item can't take so many more");
                    }
                    log_info("The party gets {} x {}", count, game.items().at(*item).name);
                });

    console.add("chests", "chests",
                "Lists the chests of the level: what they are made of, where they stand, and their "
                "items.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "chests");
                    const std::span<const Chest> chests = game.world().chests();
                    std::string list;
                    for (std::size_t i = 0; i < chests.size(); ++i) {
                        const Chest& chest = chests[i];
                        const Inventory& inventory = game.chest_inventories()[i];
                        list += std::format("\nChest {}: {} at ({:.2f}, {:.2f}, {:.2f}), {} in {}, value {}{}", i,
                                            chest_look_name(chest.look), chest.base_center.x, chest.base_center.y,
                                            chest.base_center.z, item_count(inventory.item_count()),
                                            stack_count(inventory.size()), inventory.value(game.items()),
                                            describe_items(inventory, game.items()));
                    }
                    log_info("{} chests{}", chests.size(), list);
                });

    const std::string open_usage = "open inventory|chest N";
    console.add("open", open_usage,
                "Shows the party's items, as I does, or those of chest N next to them, as activating it does.",
                [&game, &queue, &state, open_usage](std::span<const std::string> arguments) {
                    if (arguments.size() == 1 && arguments[0] == "inventory") {
                        state.inventory = InventoryRequest{};
                    } else if (arguments.size() == 2 && arguments[0] == "chest") {
                        state.inventory = InventoryRequest{.chest = parse_chest(arguments[1], game)};
                    } else {
                        throw CommandError("usage: " + open_usage);
                    }
                    // As with `menu`, the main loop opens it at the start of the next frame.
                    queue.wait(1);
                });
}

}  // namespace

Camera view_camera(const ViewSettings& view, const Player& player) {
    return view.camera.value_or(make_player_camera(player));
}

void add_game_commands(Console& console, CommandQueue& queue, Game& game, ConsoleState& state) {
    add_player_commands(console, queue, game, state);
    add_door_commands(console, game);
    add_activate_command(console, game);
    add_party_commands(console, game);
    add_item_commands(console, queue, game, state);
    add_view_commands(console, queue, state);
}

}  // namespace manifold
