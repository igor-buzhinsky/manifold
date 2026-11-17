#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec3.hpp>

#include "audio/sound.hpp"
#include "audio/sound_set.hpp"
#include "console/console.hpp"
#include "console/console_window.hpp"
#include "console/game_commands.hpp"
#include "game.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "level/texture_kind.hpp"
#include "platform/audio.hpp"
#include "platform/terminal_input.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/marker.hpp"
#include "render/overlay_renderer.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "save/saved_game.hpp"
#include "ui/crosshair.hpp"
#include "ui/debug_overlay.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/text_field.hpp"
#include "util/asset_files.hpp"
#include "util/frame_times.hpp"
#include "util/log.hpp"
#include "world/door.hpp"
#include "world/torch.hpp"

namespace {

/// Width of the window in screen units. The height follows from the 16:9 aspect ratio.
constexpr int kWindowWidth = 1600;

/// Command-line options.
struct Options {
    std::optional<std::uint64_t> seed;           ///< Level seed. Random if not given.
    std::string commands;                        ///< Console commands to run at the start. Errors stop the game.
    bool quit = false;                           ///< Whether to add `quit` after the commands.
    std::optional<manifold::LayoutKind> layout;  ///< Level layout. Random if not given.
};

/// Parses a level seed: a non-negative decimal integer.
///
/// \throws std::invalid_argument if `text` is not a valid seed.
std::uint64_t parse_seed(const std::string& text) {
    std::uint64_t seed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid seed: " + text);
    }
    return seed;
}

/// Parses the command line: `manifold [SEED] [--layout debug|rooms] [--exec COMMANDS]... [--screenshot FILE.png]`.
///
/// `--screenshot FILE.png` is short for `--exec "screenshot FILE.png"`, with `quit` after all commands.
/// Commands run in the order of the options.
///
/// \param args All arguments, including the program name.
/// \throws std::invalid_argument if an argument is invalid.
Options parse_options(std::span<char*> args) {
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--screenshot" || arg == "--exec" || arg == "--layout") {
            if (i + 1 == args.size()) {
                throw std::invalid_argument(arg + " needs a value");
            }
            const std::string value = args[++i];
            if (arg == "--screenshot") {
                options.commands += "\nscreenshot " + value;
                options.quit = true;
            } else if (arg == "--exec") {
                options.commands += "\n" + value;
            } else {
                options.layout = manifold::parse_layout_name(value);
                if (!options.layout) {
                    throw std::invalid_argument("unknown layout: " + value);
                }
            }
        } else {
            options.seed = parse_seed(arg);
        }
    }
    if (options.quit) {
        options.commands += "\nquit";
    }
    return options;
}

/// Creates an unpredictable level seed.
std::uint64_t random_seed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32U) | device();
}

/// Directory searched for level textures, set by CMake to `assets/textures` in the source tree.
constexpr const char* kTexturesDir = MANIFOLD_TEXTURES_DIR;

/// The texture of a level and what it shows.
struct LevelTexture {
    manifold::Image image;
    manifold::TextureKind kind = manifold::TextureKind::kStone;
};

/// Picks the texture of the level's surfaces at random among those of all texture kinds, see
/// manifold::find_level_textures() and manifold::pick_texture().
///
/// The choice depends only on the level seed, so a seed always gives the same look. If no texture can be
/// used, a plain grey one is used instead, as stone.
LevelTexture pick_level_texture(std::uint64_t level_seed) {
    // Mixing the seed keeps the choice independent of the random numbers used by level generation.
    constexpr std::uint64_t kTextureSeedSalt = 0x9e3779b97f4a7c15ULL;
    const std::vector<manifold::TextureFile> files = manifold::find_level_textures(kTexturesDir);
    std::vector<std::filesystem::path> candidates;
    candidates.reserve(files.size());
    for (const manifold::TextureFile& file : files) {
        candidates.push_back(file.path);
    }
    std::optional<manifold::PickedTexture> picked = manifold::pick_texture(candidates, level_seed ^ kTextureSeedSalt);
    if (!picked) {
        manifold::log_warning("No usable texture found among {} PNG files in {}, using plain grey as stone",
                              candidates.size(), kTexturesDir);
        return {.image = manifold::fallback_texture(), .kind = manifold::TextureKind::kStone};
    }
    LevelTexture texture{.image = std::move(picked->image), .kind = manifold::TextureKind::kStone};
    const std::filesystem::path path = std::move(picked->path);
    for (const manifold::TextureFile& file : files) {
        if (file.path == path) {
            texture.kind = file.kind;
        }
    }
    manifold::log_info("Texture kind: {}", manifold::texture_kind_name(texture.kind));
    return texture;
}

/// Directory searched for the sounds of the level exit portal, `assets/sounds/portal` in the source tree.
constexpr const char* kPortalSoundsDir = MANIFOLD_SOUNDS_DIR "/portal";

/// Directories of the sounds of doors that start to open and to close, in the source tree.
constexpr const char* kDoorOpenSoundsDir = MANIFOLD_SOUNDS_DIR "/door_open";
constexpr const char* kDoorCloseSoundsDir = MANIFOLD_SOUNDS_DIR "/door_close";

/// The sounds of doors, loaded once for all levels.
struct DoorSoundSets {
    manifold::SoundSet open{kDoorOpenSoundsDir, "door open"};
    manifold::SoundSet close{kDoorCloseSoundsDir, "door close"};
};

/// Plays the sounds of the doors that started to move since the last call, see manifold::Game::take_door_moves().
/// Each door has its own sounds, see manifold::DoorSounds.
void play_door_sounds(manifold::Game& game, const DoorSoundSets& sets, manifold::AudioOutput& audio) {
    for (const manifold::DoorMove& move : game.take_door_moves()) {
        const manifold::DoorSounds& sounds = game.world().doors()[move.door].sounds;
        const manifold::Sound* sound = move.open ? sets.open.get(sounds.open) : sets.close.get(sounds.close);
        if (sound != nullptr) {
            audio.play(*sound);
        }
    }
}

/// Directory of the footstep sounds, with a directory for each texture kind, e.g. `footsteps/grass`.
constexpr const char* kFootstepSoundsDir = MANIFOLD_SOUNDS_DIR "/footsteps";
/// How loud footsteps are played, relative to the files: they are scaled to the same peak as the other sounds, but
/// come often and from close by.
constexpr float kFootstepGain = 0.5f;

/// The footstep sounds of each texture kind, loaded once for all levels, and which was played last.
class FootstepSounds {
public:
    FootstepSounds() : random_(random_seed()) {
        for (const manifold::TextureKind kind : manifold::kTextureKinds) {
            const std::string name = manifold::texture_kind_name(kind);
            sets_.at(static_cast<std::size_t>(kind)) =
                manifold::SoundSet(std::filesystem::path(kFootstepSoundsDir) / name, name + " footstep");
        }
    }

    /// Plays the footsteps that the player made since the last call, see manifold::Game::take_footsteps(), with the
    /// sounds of the level's texture kind. Two steps in a row sound different, if the kind has more than one sound.
    void play(manifold::Game& game, manifold::AudioOutput& audio) {
        const int steps = game.take_footsteps();
        const manifold::SoundSet& set = sets_.at(static_cast<std::size_t>(game.level().texture_kind));
        for (int step = 0; step < steps; ++step) {
            previous_ = set.pick_other(previous_, random_());
            if (const manifold::Sound* sound = set.get(previous_)) {
                audio.play(*sound, kFootstepGain);
            }
        }
    }

private:
    std::array<manifold::SoundSet, manifold::kTextureKinds.size()> sets_;
    std::uint32_t previous_ = 0;  ///< The number of the footstep played last, see manifold::SoundSet::get().
    std::mt19937_64 random_;      ///< Picks the footsteps. Unlike the level, it doesn't depend on the seed.
};

/// Picks the sound that the level exit portal makes when the player enters it, at random among the OGG files
/// under kPortalSoundsDir, at any depth, see manifold::pick_asset_file(), and loads it.
///
/// Like the texture, the choice depends only on the level seed. The sound is logged as info.
///
/// \return The sound, or an empty optional (after logging a warning) if there is none or it can't be loaded.
std::optional<manifold::Sound> pick_exit_sound(std::uint64_t level_seed) {
    constexpr std::uint64_t kExitSoundSeedSalt = 0xd1b54a32d192ed03ULL;
    const std::vector<std::filesystem::path> candidates = manifold::find_asset_files(kPortalSoundsDir, ".ogg");
    const std::optional<std::filesystem::path> picked =
        manifold::pick_asset_file(candidates, level_seed ^ kExitSoundSeedSalt);
    if (!picked) {
        manifold::log_warning("No exit sound found in {}", kPortalSoundsDir);
        return std::nullopt;
    }
    std::optional<manifold::Sound> sound = manifold::load_sound(*picked);
    if (sound) {
        manifold::log_info("Exit sound: {} ({:.2f} s)", picked->string(), sound->duration());
    }
    return sound;
}

/// Makes the renderer of the game's current level, sets the level's ambient light in `view`, and sets the level's
/// texture kind in `game`.
///
/// \return The sound of the level's exit, see pick_exit_sound().
std::optional<manifold::Sound> start_level(std::optional<manifold::Renderer>& renderer, manifold::Game& game,
                                           manifold::ViewSettings& view) {
    const LevelTexture texture = pick_level_texture(game.seed());
    game.set_texture_kind(texture.kind);
    view.level_ambient = manifold::level_ambient(game.seed(), manifold::mean_linear_color(texture.image));
    view.lights.ambient = view.level_ambient;
    renderer.emplace(game.world(), texture.image);
    return pick_exit_sound(game.seed());
}

/// Logs the seed, the level and the player.
void log_game(const manifold::Game& game) {
    const auto& bounds = game.world().bounds();
    const auto& player = game.player();
    manifold::log_info("Manifold, level {}, seed {}, layout {}", game.level_number(), game.seed(),
                       manifold::layout_name(game.level().layout));
    manifold::log_info(
        "World: x [{}, {}], y [{}, {}], height {} m, gravity {} m/s^2, {} obstacles, {} ramps, {} torches",
        bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.height, game.level().gravity,
        game.world().obstacles().size(), game.world().ramps().size(), game.world().torches().size());
    if (!game.world().torches().empty()) {
        manifold::log_info("Torches: {} flames", manifold::torch_flame_name(game.world().torches()[0].flame));
    }
    manifold::log_info("Player: position ({:.2f}, {:.2f}, {:.2f}), radius {} m, height {} m", player.position().x,
                       player.position().y, player.position().z, player.radius(), player.height());
    manifold::log_info("Level info:\n{}", game.level().info);
}

/// Where the game is saved: `save.json` in the game's directory for user data, which SDL_GetPrefPath() gives and
/// creates: on Linux `$XDG_DATA_HOME/manifold/`, by default `~/.local/share/manifold/`.
///
/// \return The path, or nothing (after logging a warning) if there is no such directory.
std::optional<std::filesystem::path> save_file_path() {
    char* dir = SDL_GetPrefPath("", "manifold");
    if (dir == nullptr) {
        manifold::log_warning("The game can't be saved or loaded: {}", SDL_GetError());
        return std::nullopt;
    }
    std::filesystem::path path = std::filesystem::path(dir) / "save.json";
    SDL_free(dir);
    return path;
}

/// Saves the game at the start of its current level, see manifold::Game::save_point(). A failure is only logged.
void save_game(const manifold::Game& game, const std::filesystem::path& path) {
    try {
        manifold::write_save_file(path, game.save_point());
        manifold::log_info("Saved the game to {}", path.string());
    } catch (const std::runtime_error& error) {
        manifold::log_error("Can't save the game: {}", error.what());
    }
}

/// Loads the saved game into `game`, see manifold::Game::load().
///
/// \param game          The game to load into.
/// \param path          The save file, or nothing if there is no directory for saves.
/// \param stop_on_error Whether a failure is thrown, or only logged.
/// \return Whether the game was loaded.
/// \throws std::runtime_error if loading fails and `stop_on_error` is set.
bool load_game(manifold::Game& game, const std::optional<std::filesystem::path>& path, bool stop_on_error) {
    try {
        if (!path) {
            throw std::runtime_error("there is no directory for saves");
        }
        if (!std::filesystem::exists(*path)) {
            throw std::runtime_error("there is no saved game in " + path->string());
        }
        game.load(manifold::read_save_file(*path));
        manifold::log_info("Loaded the game from {}", path->string());
        return true;
    } catch (const std::runtime_error& error) {
        if (stop_on_error) {
            throw std::runtime_error(std::string("can't load the game: ") + error.what());
        }
        manifold::log_error("Can't load the game: {}", error.what());
        return false;
    }
}

/// What belongs to the game's current level outside the game: its renderer and the sound of its exit. Each level that
/// starts is saved, and the saved game can be loaded.
class LevelSession {
public:
    /// Sets up the game's current level.
    ///
    /// \param game      The game, whose level is set up.
    /// \param view      Where the level's ambient light is set.
    /// \param save_path Where the game is saved, or nothing if there is no directory for saves.
    /// \param saving    Whether to save each level that starts. If not, the save can still be loaded.
    LevelSession(manifold::Game& game, manifold::ViewSettings& view, std::optional<std::filesystem::path> save_path,
                 bool saving)
        : save_path_(std::move(save_path)), saving_(saving) {
        if (save_path_ && !saving_) {
            manifold::log_info("The game isn't saved while it runs console commands from the command line");
        }
        set_up(game, view);
    }

    /// Sets up the game's level if the game started another one since the last call, e.g. when the player reached
    /// the exit or a console command started the next level. Plays the exit sound of the level before.
    void follow(manifold::Game& game, manifold::ViewSettings& view, manifold::AudioOutput& audio) {
        if (game.level_starts() == level_starts_) {
            return;
        }
        if (exit_sound_) {
            audio.play(*exit_sound_);
        }
        set_up(game, view);
    }

    /// Loads the saved game into `game` and sets up its level. A failure is logged, and the level stays.
    ///
    /// \throws std::runtime_error if loading fails and `stop_on_error` is set.
    void load(manifold::Game& game, manifold::ViewSettings& view, bool stop_on_error) {
        if (load_game(game, save_path_, stop_on_error)) {
            set_up(game, view);
        }
    }

    /// Draws the current level. It is made by the constructor.
    ///
    /// \throws std::logic_error if it isn't, which can't happen.
    manifold::Renderer& renderer() {
        if (!renderer_) {
            throw std::logic_error("no level is set up");
        }
        return *renderer_;
    }

private:
    /// Logs the game's current level, makes its renderer and picks its exit sound, see start_level(), and saves the
    /// game if saving is on.
    void set_up(manifold::Game& game, manifold::ViewSettings& view) {
        log_game(game);
        exit_sound_ = start_level(renderer_, game, view);
        level_starts_ = game.level_starts();
        if (saving_ && save_path_) {
            save_game(game, *save_path_);
        }
    }

    std::optional<std::filesystem::path> save_path_;
    bool saving_;  ///< Whether each level that starts is saved, if there is a directory for saves.
    std::optional<manifold::Renderer> renderer_;  ///< The renderer has the meshes of one level.
    std::optional<manifold::Sound> exit_sound_;
    int level_starts_ = 0;  ///< manifold::Game::level_starts() of the level that is set up.
};

/// The key that opens and closes the console: the one left of 1, labelled `/~ on US keyboards.
constexpr SDL_Scancode kConsoleKey = SDL_SCANCODE_GRAVE;
/// The key that shows and hides the debug overlay.
constexpr SDL_Keycode kDebugOverlayKey = SDLK_F3;
/// The key that loads the saved game. Like the movement keys, it is the key at the place of L on US keyboards.
constexpr SDL_Scancode kLoadKey = SDL_SCANCODE_L;

/// The key of the console window that `key` is, if any.
std::optional<manifold::ConsoleKey> console_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return manifold::ConsoleKey::kSubmit;
        case SDLK_UP:
            return manifold::ConsoleKey::kHistoryBack;
        case SDLK_DOWN:
            return manifold::ConsoleKey::kHistoryForward;
        case SDLK_PAGEUP:
            return manifold::ConsoleKey::kScrollUp;
        case SDLK_PAGEDOWN:
            return manifold::ConsoleKey::kScrollDown;
        default:
            return std::nullopt;
    }
}

/// The key that edits a text field that `key` is, if any.
std::optional<manifold::EditKey> edit_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_BACKSPACE:
            return manifold::EditKey::kBackspace;
        case SDLK_DELETE:
            return manifold::EditKey::kDelete;
        case SDLK_LEFT:
            return manifold::EditKey::kLeft;
        case SDLK_RIGHT:
            return manifold::EditKey::kRight;
        case SDLK_HOME:
            return manifold::EditKey::kHome;
        case SDLK_END:
            return manifold::EditKey::kEnd;
        default:
            return std::nullopt;
    }
}

/// Opens or closes the console window, with text input only while it is open.
void set_console_open(manifold::ConsoleWindow& console_window, manifold::Window& window, bool open) {
    console_window.set_open(open);
    window.set_text_input(open);
}

/// Handles a key press while the console window is open: editing, history, scrolling and running commands.
void handle_console_key(const SDL_KeyboardEvent& key, manifold::ConsoleWindow& console_window,
                        manifold::CommandQueue& commands) {
    if (const std::optional<manifold::EditKey> edit = edit_key(key.key)) {
        console_window.input().apply(*edit);
    } else if (const std::optional<manifold::ConsoleKey> console = console_key(key.key)) {
        if (const std::optional<std::string> command = console_window.handle(*console)) {
            commands.add(*command, false);
        }
    }
}

/// Handles a key press: the console key, Escape, the keys of the console window while it is open, and otherwise
/// the load key, which asks `state` to load the game.
///
/// \return False when the game should quit.
bool handle_key_down(const SDL_KeyboardEvent& key, manifold::Window& window, manifold::ConsoleWindow& console_window,
                     manifold::CommandQueue& commands, manifold::ConsoleState& state) {
    if (key.scancode == kConsoleKey) {
        if (!key.repeat) {
            set_console_open(console_window, window, !console_window.is_open());
        }
    } else if (key.key == SDLK_ESCAPE) {
        if (!key.repeat) {
            if (!console_window.is_open()) {
                return false;
            }
            set_console_open(console_window, window, false);
        }
    } else if (console_window.is_open()) {
        handle_console_key(key, console_window, commands);
    } else if (key.scancode == kLoadKey && !key.repeat) {
        state.load = true;
        state.load_stops_on_error = false;
    }
    return true;
}

/// Handles pending window and key events: quitting, the console window, the debug overlay and loading.
///
/// These are key bindings, like those of read_input(). Keep GAME_MANUAL.md up to date when changing them.
///
/// \param window             The window, whose text input is on while the console window is open.
/// \param console_window     The console window.
/// \param commands           Where commands run in the console window go.
/// \param overlay            The debug overlay, which gets the events first and may keep keys from the game.
/// \param state              Where the debug overlay key shows or hides the overlay, and the load key asks to load.
/// \param console_key_held   Whether the console key is held down. Kept between calls.
/// \return False when the game should quit.
bool handle_events(manifold::Window& window, manifold::ConsoleWindow& console_window, manifold::CommandQueue& commands,
                   manifold::DebugOverlay& overlay, manifold::ConsoleState& state, bool& console_key_held) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        // The debug overlay key works even while ImGui has the keyboard, so the overlay can always be hidden.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == kDebugOverlayKey) {
            if (!event.key.repeat) {
                state.debug_overlay = !state.debug_overlay;
            }
            continue;
        }
        if (overlay.handle_event(event)) {
            continue;
        }
        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.scancode == kConsoleKey) {
            console_key_held = event.type == SDL_EVENT_KEY_DOWN;
        }
        if (event.type == SDL_EVENT_TEXT_INPUT && console_window.is_open()) {
            // The console key types a character, which depends on the keyboard layout. It would land in the
            // input line, so text is ignored while the key is held.
            if (!console_key_held) {
                console_window.input().insert(event.text.text);
            }
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !handle_key_down(event.key, window, console_window, commands, state)) {
            return false;
        }
    }
    return true;
}

/// Frames longer than this, e.g. after the window was dragged, count as this long to avoid jumps.
constexpr float kMaxFrameTime = 0.1f;

/// Reads the keys that are held down.
///
/// These are the key bindings, with those of handle_events(). Keep GAME_MANUAL.md up to date when changing
/// them.
manifold::PlayerInput read_input() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
        return (keys[positive] ? 1.0f : 0.0f) - (keys[negative] ? 1.0f : 0.0f);
    };
    return {
        .turn = axis(SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT),
        .look = axis(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN),
        .forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S),
        .strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A),
        .rise = axis(SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN),
        .activate = keys[SDL_SCANCODE_E],
    };
}

/// Draws the game into the bound framebuffer, as `view` shows it, with the crosshair if the view has it.
///
/// \param renderer         Draws the world.
/// \param overlay_renderer Draws the crosshair.
/// \param game             What to draw.
/// \param view             How to show it.
/// \param framebuffer      Size of the bound framebuffer. The picture is centered in it, see fit_viewport().
void draw_game(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
               const manifold::Game& game, const manifold::ViewSettings& view, const manifold::PixelSize& framebuffer) {
    // The light spell at the player's eyes, the level's ambient light if any, and the lights of the torches and the
    // exit nearest to the camera.
    const manifold::Camera camera = manifold::view_camera(view, game.player());
    manifold::Lighting lighting =
        manifold::scene_lighting(view.lights, game.player().eye_position(), game.world().torches(), game.world().exit(),
                                 game.animation_time(), camera.position);
    if (!view.shadows) {
        for (manifold::PointLight& light : lighting.lights) {
            light.shadow_id.reset();
        }
    }

    const std::vector<glm::vec3> arrow =
        view.arrow ? manifold::build_arrow(game.player().position(), game.player().yaw()) : std::vector<glm::vec3>();
    const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
    renderer.draw(
        camera, lighting, viewport, game.animation_time(), game.doors(),
        {.lighting = view.lighting, .marker = arrow, .cut_height = view.cutaway, .shadow_bias = view.shadow_bias});
    // The crosshair shows where the player looks, which a camera put in place doesn't.
    if (view.crosshair && !view.camera) {
        manifold::OverlayMesh crosshair;
        manifold::add_crosshair(crosshair, {.width = viewport.width, .height = viewport.height});
        overlay_renderer.draw(crosshair, viewport);
    }
}

/// Takes the screenshots that console commands asked for, each drawn as it asked.
///
/// A screenshot that fails is logged, and stops the game only if its request says so.
///
/// \param window_size The size of screenshots that don't give their own.
/// \throws std::runtime_error if a screenshot that stops on error fails.
void take_screenshots(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
                      const manifold::Game& game, std::span<const manifold::ScreenshotRequest> requests,
                      const manifold::PixelSize& window_size) {
    for (const manifold::ScreenshotRequest& request : requests) {
        const manifold::PixelSize size = request.size.value_or(window_size);
        try {
            const manifold::OffscreenFramebuffer framebuffer(size);
            draw_game(renderer, overlay_renderer, game, request.view, size);
            manifold::save_screenshot(request.path, size.width, size.height);
            manifold::log_info("Saved {} ({} x {})", request.path, size.width, size.height);
        } catch (const std::runtime_error& error) {
            if (request.stop_on_error) {
                throw;
            }
            manifold::log_error("screenshot {}: {}", request.path, error.what());
        }
    }
}

/// Removes the log listener when it goes out of scope, before what the listener uses is gone.
struct LogListenerReset {
    LogListenerReset() = default;
    ~LogListenerReset() { manifold::set_log_listener({}); }
    LogListenerReset(const LogListenerReset&) = delete;
    LogListenerReset& operator=(const LogListenerReset&) = delete;
    LogListenerReset(LogListenerReset&&) = delete;
    LogListenerReset& operator=(LogListenerReset&&) = delete;
};

/// Runs the game until the player quits or a console command does.
///
/// \throws manifold::CommandError if one of the commands of `options` fails.
/// \throws std::runtime_error if a level can't be generated, also the next one when the player reaches the exit.
void run(const Options& options) {
    // The console window shows the log, from the start.
    manifold::ConsoleWindow console_window;
    manifold::set_log_listener([&console_window](manifold::LogLevel level, std::string_view message) {
        console_window.add_output(level, message);
    });
    const LogListenerReset reset_log_listener;

    manifold::Game game(manifold::GameConfig{}, options.seed.value_or(random_seed()), options.layout);

    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height);
    manifold::ConsoleState state;
    manifold::AudioOutput audio;
    const DoorSoundSets door_sounds;
    FootstepSounds footsteps;
    const manifold::OverlayRenderer overlay_renderer;
    // Each level is saved when it starts, which replaces the save of the level before. Scripts of console commands
    // mostly test the game, so they leave the player's save alone.
    LevelSession level(game, state.view, save_file_path(), options.commands.empty());

    manifold::Console console;
    manifold::CommandQueue commands;
    manifold::add_game_commands(console, commands, game, state);
    commands.add(options.commands, true);
    manifold::TerminalInput terminal;
    manifold::DebugOverlay overlay(window);
    manifold::FrameTimes frame_times;

    bool console_key_held = false;
    std::uint64_t previous_ticks = SDL_GetTicksNS();
    do {
        // Loading comes before the commands, so that those after `load` run in the loaded level.
        if (std::exchange(state.load, false)) {
            level.load(game, state.view, state.load_stops_on_error);
        }
        for (const std::string& line : terminal.read_lines()) {
            commands.add(line, false);
        }
        commands.run_frame(console);

        const std::uint64_t ticks = SDL_GetTicksNS();
        const float frame_time = static_cast<float>(ticks - previous_ticks) * 1e-9f;
        previous_ticks = ticks;
        frame_times.add(frame_time);
        // While the console window is open, or the debug overlay has the keyboard, the keys type into them
        // instead of moving the player.
        const bool keys_taken = console_window.is_open() || overlay.wants_keyboard();
        manifold::PlayerInput input = keys_taken ? manifold::PlayerInput{} : read_input();
        if (state.walk_frames > 0) {
            input.forward = 1.0f;
            --state.walk_frames;
        }
        game.update(input, std::min(frame_time, kMaxFrameTime));
        // The player reached the exit, or a console command started the next level as if they had.
        level.follow(game, state.view, audio);
        play_door_sounds(game, door_sounds, audio);
        footsteps.play(game, audio);

        const manifold::PixelSize window_size = window.framebuffer_size();
        draw_game(level.renderer(), overlay_renderer, game, state.view, window_size);
        // The stats of this draw, before screenshots draw again.
        const manifold::RenderStats render_stats = level.renderer().stats();
        if (state.debug_overlay) {
            overlay.draw(game, state.view, render_stats, frame_times);
        }
        if (console_window.is_open()) {
            const manifold::Viewport viewport = manifold::fit_viewport(window_size, manifold::kAspectRatio);
            overlay_renderer.draw(console_window.build_mesh({.width = viewport.width, .height = viewport.height}),
                                  viewport);
        }
        take_screenshots(level.renderer(), overlay_renderer, game, state.screenshots, window_size);
        state.screenshots.clear();
        if (state.quit) {
            return;
        }
        window.swap_buffers();
        // The console window has the keys while it is open, so the overlay gets no events then.
        overlay.set_input(state.debug_overlay && !console_window.is_open());
    } while (handle_events(window, console_window, commands, overlay, state, console_key_held));
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        run(parse_options(std::span(argv, argc)));
    } catch (const std::exception& error) {
        manifold::log_error("{}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
