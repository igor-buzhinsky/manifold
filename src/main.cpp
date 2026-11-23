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
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec2.hpp>
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
#include "party/party.hpp"
#include "party/spells.hpp"
#include "platform/audio.hpp"
#include "platform/terminal_input.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/gl_util.hpp"
#include "render/image_renderer.hpp"
#include "render/lighting.hpp"
#include "render/marker.hpp"
#include "render/overlay_renderer.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "save/saved_game.hpp"
#include "settings/settings.hpp"
#include "ui/crosshair.hpp"
#include "ui/debug_overlay.hpp"
#include "ui/game_menus.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/party_hud.hpp"
#include "ui/screen.hpp"
#include "ui/screen_stack.hpp"
#include "ui/spell_hud.hpp"
#include "ui/text_field.hpp"
#include "ui/ui_event.hpp"
#include "util/asset_files.hpp"
#include "util/frame_times.hpp"
#include "util/log.hpp"
#include "util/random.hpp"
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

/// The directories of the game's textures, sounds and portraits.
struct AssetDirs {
    std::filesystem::path textures;   ///< Searched for level textures.
    std::filesystem::path sounds;     ///< Has a directory for each kind of sound, e.g. `portal`.
    std::filesystem::path portraits;  ///< Searched for portraits, see manifold::find_portraits().
};

/// Finds the game's assets, see manifold::find_assets_dir(): the `assets` directory next to the executable, or else
/// the one in the source tree, whose path CMake sets. Logs which is used.
AssetDirs find_asset_dirs() {
    // SDL keeps the string, so it isn't freed.
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        manifold::log_warning("The directory of the executable isn't known: {}", SDL_GetError());
    }
    const std::filesystem::path assets = manifold::find_assets_dir(
        base_path != nullptr ? std::filesystem::path(base_path) : std::filesystem::path(), MANIFOLD_ASSETS_DIR);
    manifold::log_info("Assets: {}", assets.string());
    return {.textures = assets / "textures", .sounds = assets / "sounds", .portraits = assets / "portraits"};
}

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
LevelTexture pick_level_texture(std::uint64_t level_seed, const std::filesystem::path& textures_dir) {
    // Mixing the seed keeps the choice independent of the random numbers used by level generation.
    constexpr std::uint64_t kTextureSeedSalt = 0x9e3779b97f4a7c15ULL;
    const std::vector<manifold::TextureFile> files = manifold::find_level_textures(textures_dir);
    std::vector<std::filesystem::path> candidates;
    candidates.reserve(files.size());
    for (const manifold::TextureFile& file : files) {
        candidates.push_back(file.path);
    }
    std::optional<manifold::PickedTexture> picked = manifold::pick_texture(candidates, level_seed ^ kTextureSeedSalt);
    if (!picked) {
        manifold::log_warning("No usable texture found among {} PNG files in {}, using plain grey as stone",
                              candidates.size(), textures_dir.string());
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

/// Directory in AssetDirs::sounds searched for the sounds of the level exit portal.
constexpr const char* kPortalSoundsDir = "portal";

/// Directories in AssetDirs::sounds of the sounds of doors that start to open and to close.
constexpr const char* kDoorOpenSoundsDir = "door_open";
constexpr const char* kDoorCloseSoundsDir = "door_close";

/// The sounds of doors, loaded once for all levels.
struct DoorSoundSets {
    explicit DoorSoundSets(const std::filesystem::path& sounds_dir)
        : open(sounds_dir / kDoorOpenSoundsDir, "door open"), close(sounds_dir / kDoorCloseSoundsDir, "door close") {}

    manifold::SoundSet open;
    manifold::SoundSet close;
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

/// Directory in AssetDirs::sounds of the footstep sounds, with a directory for each texture kind, e.g.
/// `footsteps/grass`.
constexpr const char* kFootstepSoundsDir = "footsteps";
/// How loud footsteps are played, relative to the files: they are scaled to the same peak as the other sounds, but
/// come often and from close by.
constexpr float kFootstepGain = 0.5f;

/// The footstep sounds of each texture kind, loaded once for all levels, and which was played last.
class FootstepSounds {
public:
    explicit FootstepSounds(const std::filesystem::path& sounds_dir) : random_(random_seed()) {
        for (const manifold::TextureKind kind : manifold::kTextureKinds) {
            const std::string name = manifold::texture_kind_name(kind);
            sets_.at(static_cast<std::size_t>(kind)) =
                manifold::SoundSet(sounds_dir / kFootstepSoundsDir / name, name + " footstep");
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
    manifold::Rng random_;        ///< Picks the footsteps. Unlike the level, it doesn't depend on the seed.
};

/// Picks the sound that the level exit portal makes when the player enters it, at random among the OGG files
/// under kPortalSoundsDir in `sounds_dir`, at any depth, see manifold::pick_asset_file(), and loads it.
///
/// Like the texture, the choice depends only on the level seed. The sound is logged as info.
///
/// \return The sound, or an empty optional (after logging a warning) if there is none or it can't be loaded.
std::optional<manifold::Sound> pick_exit_sound(std::uint64_t level_seed, const std::filesystem::path& sounds_dir) {
    constexpr std::uint64_t kExitSoundSeedSalt = 0xd1b54a32d192ed03ULL;
    const std::filesystem::path portal_dir = sounds_dir / kPortalSoundsDir;
    const std::vector<std::filesystem::path> candidates = manifold::find_asset_files(portal_dir, ".ogg");
    const std::optional<std::filesystem::path> picked =
        manifold::pick_asset_file(candidates, level_seed ^ kExitSoundSeedSalt);
    if (!picked) {
        manifold::log_warning("No exit sound found in {}", portal_dir.string());
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
                                           manifold::ViewSettings& view, const AssetDirs& assets) {
    const LevelTexture texture = pick_level_texture(game.seed(), assets.textures);
    game.set_texture_kind(texture.kind);
    view.level_ambient = manifold::level_ambient(game.seed(), manifold::mean_linear_color(texture.image));
    view.lights.ambient = view.level_ambient;
    renderer.emplace(game.world(), texture.image);
    return pick_exit_sound(game.seed(), assets.sounds);
}

/// The images of the portraits of the party, which are loaded again whenever the party gets other portraits, and draws
/// them with the bars of hit points, see manifold::layout_party_hud().
///
/// Needs a current OpenGL 4.1 core context, see manifold::Window for its whole lifetime.
class PartyPortraits {
public:
    /// \param portraits_dir The directory that manifold::Character::portrait is relative to.
    explicit PartyPortraits(std::filesystem::path portraits_dir) : portraits_dir_(std::move(portraits_dir)) {}

    /// Loads the images of the portraits of `party` if they are not those loaded last. A portrait that can't be loaded
    /// is logged as a warning and shows as an empty place, as does a character without one.
    void follow(std::span<const manifold::Character> party) {
        std::vector<std::string> portraits;
        portraits.reserve(party.size());
        for (const manifold::Character& character : party) {
            portraits.push_back(character.portrait);
        }
        if (portraits == portraits_) {
            return;
        }
        images_.clear();
        for (const std::string& portrait : portraits) {
            std::optional<manifold::ImageTexture>& texture = images_.emplace_back();
            if (portrait.empty()) {
                continue;
            }
            if (const std::optional<manifold::Image> image = manifold::load_image(portraits_dir_ / portrait)) {
                texture.emplace(*image);
            }
        }
        portraits_ = std::move(portraits);
    }

    /// Draws the portraits of `party`, which follow() was called for, and their bars into `viewport`.
    void draw(std::span<const manifold::Character> party, const manifold::OverlayRenderer& overlay_renderer,
              const manifold::Viewport& viewport) const {
        const std::vector<manifold::CharacterPlacement> placements =
            manifold::layout_party_hud({.width = viewport.width, .height = viewport.height}, party.size());
        manifold::OverlayMesh mesh;
        manifold::add_party_hud(mesh, placements, party);
        overlay_renderer.draw(mesh, viewport);
        for (std::size_t i = 0; i < placements.size() && i < images_.size(); ++i) {
            const std::optional<manifold::ImageTexture>& image = images_[i];
            if (image.has_value()) {
                image_renderer_.draw(*image, placements[i].portrait, viewport);
            }
        }
    }

private:
    std::filesystem::path portraits_dir_;
    manifold::ImageRenderer image_renderer_;
    std::vector<std::string> portraits_;  ///< manifold::Character::portrait of each character loaded last.
    /// The image of each character loaded last, or nothing if it has none.
    std::vector<std::optional<manifold::ImageTexture>> images_;
};

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

/// The game's directory for user data, of the save and the player's settings, which SDL_GetPrefPath() gives and
/// creates: on Linux `$XDG_DATA_HOME/manifold/`, by default `~/.local/share/manifold/`.
///
/// \return The directory, or nothing (after logging a warning) if there is no such directory.
std::optional<std::filesystem::path> user_data_dir() {
    char* dir = SDL_GetPrefPath("", "manifold");
    if (dir == nullptr) {
        manifold::log_warning("The game's directory for user data isn't known: {}", SDL_GetError());
        return std::nullopt;
    }
    std::filesystem::path path(dir);
    SDL_free(dir);
    return path;
}

/// Where the game is saved: `save.json` in the game's directory for user data, see user_data_dir().
///
/// \return The path, or nothing (after logging a warning) if there is no such directory.
std::optional<std::filesystem::path> save_file_path(const std::optional<std::filesystem::path>& data_dir) {
    if (!data_dir) {
        manifold::log_warning("The game can't be saved or loaded");
        return std::nullopt;
    }
    return *data_dir / "save.json";
}

/// Reads the settings: the game's own (see manifold::find_game_settings_file()), with the player's in the game's
/// directory for user data over them, see manifold::load_settings().
///
/// \param data_dir See user_data_dir().
manifold::Settings read_settings(const std::optional<std::filesystem::path>& data_dir) {
    // SDL keeps the string, so it isn't freed.
    const char* base_path = SDL_GetBasePath();
    std::vector<std::filesystem::path> files = {manifold::find_game_settings_file(
        base_path != nullptr ? std::filesystem::path(base_path) : std::filesystem::path(), MANIFOLD_SETTINGS_FILE)};
    if (data_dir) {
        files.push_back(*data_dir / manifold::kSettingsFileName);
    }
    return manifold::load_settings(files);
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

/// Whether there is a save at `path` that can be read, e.g. to offer to continue it. A save that can't be read is
/// logged as a warning.
bool has_readable_save(const std::optional<std::filesystem::path>& path) {
    std::error_code error;
    if (!path || !std::filesystem::exists(*path, error)) {
        return false;
    }
    try {
        manifold::read_save_file(*path);
        return true;
    } catch (const std::runtime_error& read_error) {
        manifold::log_warning("The saved game can't be read, so a new game starts: {}", read_error.what());
        return false;
    }
}

/// What belongs to the game's current level outside the game: its renderer and the sound of its exit. If saving is on,
/// each level that starts is saved. The saved game can be loaded either way.
class LevelSession {
public:
    /// Sets up the game's current level.
    ///
    /// \param game      The game, whose level is set up.
    /// \param view      Where the level's ambient light is set.
    /// \param assets    Where the level's texture and exit sound are picked from.
    /// \param save_path Where the game is saved, or nothing if there is no directory for saves.
    /// \param saving    Whether to save each level that starts. If not, the save can still be loaded.
    /// \param defer     Whether to save the game's level only when save_deferred() is called, e.g. after the player
    ///                  chose not to continue the save that is there, instead of now.
    LevelSession(manifold::Game& game, manifold::ViewSettings& view, AssetDirs assets,
                 std::optional<std::filesystem::path> save_path, bool saving, bool defer)
        : assets_(std::move(assets)), save_path_(std::move(save_path)), saving_(saving) {
        if (save_path_ && !saving_) {
            manifold::log_info(
                "The game isn't saved in this run, since it runs console commands from the command line");
        }
        set_up(game, view, !defer);
    }

    /// Sets up the game's level if the game started another one since the last call, e.g. when the player reached
    /// the exit or a console command started the next level. Plays the exit sound of the level before.
    ///
    /// If the level before waited for save_deferred(), so does the new one, so that the save offered by the start menu
    /// stays until the player chooses, e.g. when `next_level` is run over the start menu.
    void follow(manifold::Game& game, manifold::ViewSettings& view, manifold::AudioOutput& audio) {
        if (game.level_starts() == level_starts_) {
            return;
        }
        if (exit_sound_) {
            audio.play(*exit_sound_);
        }
        set_up(game, view, !deferred_);
    }

    /// Loads the saved game into `game` and sets up its level. Unless `stop_on_error` is set, a failure is only logged,
    /// and the level stays.
    ///
    /// \return Whether the game was loaded.
    /// \throws std::runtime_error if loading fails and `stop_on_error` is set.
    bool load(manifold::Game& game, manifold::ViewSettings& view, bool stop_on_error) {
        if (!load_game(game, save_path_, stop_on_error)) {
            return false;
        }
        set_up(game, view);
        return true;
    }

    /// Starts a new game, which replaces the save, with a random seed and `layout` for its first level, see
    /// manifold::Game::new_game(), and sets up its level.
    ///
    /// If the level of the game was set up without saving it (see the constructor), nothing has happened in it yet, so
    /// it is kept as the new game and saved now. This keeps a seed given on the command line.
    ///
    /// \return Whether the new game started. A failure is logged, and the level stays.
    bool new_game(manifold::Game& game, manifold::ViewSettings& view, std::optional<manifold::LayoutKind> layout) {
        if (deferred_) {
            save_deferred(game);
            return true;
        }
        try {
            game.new_game(random_seed(), layout);
        } catch (const std::runtime_error& error) {
            manifold::log_error("Can't start a new game: {}", error.what());
            return false;
        }
        manifold::log_info("Started a new game");
        set_up(game, view);
        return true;
    }

    /// Saves the game's level if it was set up without saving it (see the constructor) and saving is on.
    void save_deferred(const manifold::Game& game) {
        if (std::exchange(deferred_, false)) {
            save(game);
        }
    }

    /// Whether there is a save to load. It may still fail to load.
    bool has_save() const {
        std::error_code error;
        return save_path_ && std::filesystem::exists(*save_path_, error);
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
    /// game if saving is on, now or, if `save_now` is false, when save_deferred() is called.
    void set_up(manifold::Game& game, manifold::ViewSettings& view, bool save_now = true) {
        log_game(game);
        exit_sound_ = start_level(renderer_, game, view, assets_);
        level_starts_ = game.level_starts();
        deferred_ = !save_now;
        if (save_now) {
            save(game);
        }
    }

    /// Saves the game if saving is on.
    void save(const manifold::Game& game) const {
        if (saving_ && save_path_) {
            save_game(game, *save_path_);
        }
    }

    AssetDirs assets_;
    std::optional<std::filesystem::path> save_path_;
    bool saving_;  ///< Whether each level that starts is saved, if there is a directory for saves.
    std::optional<manifold::Renderer> renderer_;  ///< The renderer has the meshes of one level.
    std::optional<manifold::Sound> exit_sound_;
    int level_starts_ = 0;   ///< manifold::Game::level_starts() of the level that is set up.
    bool deferred_ = false;  ///< Whether the level that is set up waits for save_deferred() to be saved.
};

/// The key that opens and closes the console: the one left of 1, labelled `/~ on US keyboards.
constexpr SDL_Scancode kConsoleKey = SDL_SCANCODE_GRAVE;
/// The key that shows and hides the debug overlay.
constexpr SDL_Keycode kDebugOverlayKey = SDLK_F3;
/// The key that loads the saved game.
constexpr SDL_Keycode kLoadKey = SDLK_F9;
/// The keys that cast spells. Like the movement keys, they are the keys at the places of C and L on US keyboards.
constexpr SDL_Scancode kCandlelightKey = SDL_SCANCODE_C;
constexpr SDL_Scancode kLevitateKey = SDL_SCANCODE_L;

/// The spell that `key` casts, if any.
std::optional<manifold::Spell> spell_key(SDL_Scancode key) {
    switch (key) {
        case kCandlelightKey:
            return manifold::Spell::kCandlelight;
        case kLevitateKey:
            return manifold::Spell::kLevitate;
        default:
            return std::nullopt;
    }
}
/// With Ctrl, the key that quits the game at once. It is the key labelled Q, wherever it is, as is usual for shortcuts.
constexpr SDL_Keycode kQuitKey = SDLK_Q;

/// Whether `key` is the shortcut that quits the game: Ctrl+Q.
bool is_quit_shortcut(const SDL_KeyboardEvent& key) { return key.key == kQuitKey && (key.mod & SDL_KMOD_CTRL) != 0; }

/// The game's interface: the screens that are open, such as a menu, and what they asked for in this frame.
struct Ui {
    manifold::ScreenStack screens;
    /// The first command that the screens gave in this frame, which the main loop carries out at the start of the
    /// next frame, see apply_ui_command(). Later events of the frame don't reach the screens, since the command may
    /// close them.
    std::optional<manifold::UiCommand> command;
    manifold::PixelSize picture;  ///< Size of the picture that the screens were drawn over last.
    /// Where the left button was pressed over the game in this frame while no screen was open, as fractions of the
    /// picture's width and height from its top-left corner, see Camera::ray_direction(). Presses on the black bars
    /// are left out.
    std::optional<glm::vec2> game_click;
    /// The spells whose keys were pressed in this frame while no screen was open, in order.
    std::vector<manifold::Spell> casts;

    /// Passes an event to the screen on top, unless the screens already asked for something in this frame.
    void handle(const manifold::UiEvent& event) {
        if (!command) {
            command = screens.handle(event, picture);
        }
    }
};

/// The key of the interface's screens that `key` is, if any, besides Escape, which is handled on its own.
std::optional<manifold::NavKey> nav_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
            return manifold::NavKey::kUp;
        case SDLK_DOWN:
            return manifold::NavKey::kDown;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            return manifold::NavKey::kConfirm;
        default:
            return std::nullopt;
    }
}

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

/// Handles a key press: the console key; Escape, which closes the console, leaves the screen on top, or asks `state`
/// for the game menu; the keys of the console window while it is open; those of the screen on top while one is open;
/// and otherwise the load key, which asks `state` to load the game, and the keys of spells, which are kept in
/// `ui.casts`.
void handle_key_down(const SDL_KeyboardEvent& key, manifold::Window& window, manifold::ConsoleWindow& console_window,
                     manifold::CommandQueue& commands, manifold::ConsoleState& state, Ui& ui) {
    if (key.scancode == kConsoleKey) {
        if (!key.repeat) {
            set_console_open(console_window, window, !console_window.is_open());
        }
    } else if (key.key == SDLK_ESCAPE) {
        if (key.repeat) {
            return;
        }
        if (console_window.is_open()) {
            set_console_open(console_window, window, false);
        } else if (!ui.screens.empty()) {
            ui.handle(manifold::NavKey::kCancel);
        } else {
            state.menu = manifold::MenuRequest::kGame;
        }
    } else if (console_window.is_open()) {
        handle_console_key(key, console_window, commands);
    } else if (!ui.screens.empty()) {
        // Holding Up or Down moves on, but holding Enter doesn't choose again.
        const std::optional<manifold::NavKey> nav = nav_key(key.key);
        if (nav && (!key.repeat || *nav != manifold::NavKey::kConfirm)) {
            ui.handle(*nav);
        }
    } else if (key.repeat) {
        return;
    } else if (key.key == kLoadKey) {
        state.load = true;
        state.load_stops_on_error = false;
    } else if (const std::optional<manifold::Spell> spell = spell_key(key.scancode)) {
        ui.casts.push_back(*spell);
    }
}

/// The point of the picture under a point of the window, in pixels from its top-left corner, y down.
///
/// The picture is laid out for the current size of the window, not that of the last drawing, in case the window was
/// resized. Also updates `ui.picture` to its size.
glm::vec2 picture_point(const glm::vec2& point, const manifold::Window& window, Ui& ui) {
    const manifold::PixelSize framebuffer = window.framebuffer_size();
    const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
    ui.picture = {.width = viewport.width, .height = viewport.height};
    return manifold::window_to_viewport(point, window.size(), framebuffer, viewport);
}

/// Passes a mouse event to the screen on top as a pointer event, in pixels of the picture, if it is a motion or the
/// left button and a screen is open. Without a screen, a press of the left button over the picture is kept in
/// `ui.game_click`, to activate what is there, unless it is on the portraits of the party, the spells in effect or the
/// bar of mana, see manifold::party_hud_contains() and manifold::spell_hud_contains().
///
/// \param game The game, whose party and magic are shown over it.
void handle_mouse_event(const SDL_Event& event, const manifold::Window& window, Ui& ui, const manifold::Game& game) {
    if (ui.screens.empty()) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            const glm::vec2 point = picture_point({event.button.x, event.button.y}, window, ui);
            const glm::vec2 size{static_cast<float>(ui.picture.width), static_cast<float>(ui.picture.height)};
            if (point.x >= 0.0f && point.y >= 0.0f && point.x < size.x && point.y < size.y &&
                !manifold::party_hud_contains(ui.picture, game.party().size(), point) &&
                !manifold::spell_hud_contains(ui.picture, game.magic(), point)) {
                ui.game_click = point / size;
            }
        }
        return;
    }
    std::optional<manifold::PointerEvent> pointer;
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        pointer = manifold::PointerEvent{.action = manifold::PointerAction::kMove,
                                         .position = {event.motion.x, event.motion.y}};
    } else if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
               event.button.button == SDL_BUTTON_LEFT) {
        pointer = manifold::PointerEvent{.action = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                                                       ? manifold::PointerAction::kPress
                                                       : manifold::PointerAction::kRelease,
                                         .position = {event.button.x, event.button.y}};
    }
    if (!pointer) {
        return;
    }
    pointer->position = picture_point(pointer->position, window, ui);
    ui.handle(*pointer);
}

/// Keeps track of whether the console key is held, and types text into the console window while it is open.
///
/// \param console_key_held Whether the console key is held down. Kept between calls.
/// \return Whether the event was text typed while the console window is open, which nothing else should handle.
bool handle_console_text(const SDL_Event& event, manifold::ConsoleWindow& console_window, bool& console_key_held) {
    if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.scancode == kConsoleKey) {
        console_key_held = event.type == SDL_EVENT_KEY_DOWN;
    }
    if (event.type != SDL_EVENT_TEXT_INPUT || !console_window.is_open()) {
        return false;
    }
    // The console key types a character, which depends on the keyboard layout. It would land in the input line, so
    // text is ignored while the key is held.
    if (!console_key_held) {
        console_window.input().insert(event.text.text);
    }
    return true;
}

/// Handles pending window, key and mouse events: quitting, the console window, the debug overlay, the screens of the
/// interface, loading and casting spells.
///
/// These are key bindings, like those of read_input(). Keep GAME_MANUAL.md up to date when changing them.
///
/// \param window             The window, whose text input is on while the console window is open.
/// \param console_window     The console window.
/// \param commands           Where commands run in the console window go.
/// \param overlay            The debug overlay, which gets the events first and may keep keys from the game.
/// \param state              Where the debug overlay key shows or hides the overlay, the load key asks to load, and
///                           Escape asks for the game menu.
/// \param ui                 The screens, which get keys and the mouse while one is open and the console isn't.
/// \param console_key_held   Whether the console key is held down. Kept between calls.
/// \param game               The game, whose portraits, spells and mana clicks don't reach the game through.
/// \return False when the game should quit.
bool handle_events(manifold::Window& window, manifold::ConsoleWindow& console_window, manifold::CommandQueue& commands,
                   manifold::DebugOverlay& overlay, manifold::ConsoleState& state, Ui& ui, bool& console_key_held,
                   const manifold::Game& game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Like closing the window, the quit shortcut works even while ImGui or the console has the keyboard.
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && is_quit_shortcut(event.key))) {
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
        if (handle_console_text(event, console_window, console_key_held)) {
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            handle_key_down(event.key, window, console_window, commands, state, ui);
        } else if (!console_window.is_open()) {
            handle_mouse_event(event, window, ui, game);
        }
    }
    return true;
}

/// Frames longer than this, e.g. after the window was dragged, count as this long to avoid jumps.
constexpr float kMaxFrameTime = 0.1f;

/// Reads the keys that are held down, and the click and the spells cast in this frame over the game.
///
/// These are the key bindings, with those of handle_events(). Keep GAME_MANUAL.md up to date when changing
/// them.
///
/// \param ui     Where the left button was pressed over the game, see Ui::game_click, and the spells cast, see
///               Ui::casts.
/// \param view   How the game is shown. While a camera put in place shows the picture, clicks are ignored, since they
///               activate what the player's eyes see at that point of the picture.
/// \param player The player whose eyes the click activates from.
manifold::PlayerInput read_input(const Ui& ui, const manifold::ViewSettings& view, const manifold::Player& player) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
        return (keys[positive] ? 1.0f : 0.0f) - (keys[negative] ? 1.0f : 0.0f);
    };
    std::optional<glm::vec3> click;
    if (ui.game_click && !view.camera) {
        click = manifold::make_player_camera(player).ray_direction(*ui.game_click);
    }
    return {
        .turn = axis(SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT),
        .look = axis(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN),
        .forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S),
        .run = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT],
        .strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A),
        .rise = axis(SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN),
        .activate = keys[SDL_SCANCODE_E],
        .click = click,
        .casts = ui.casts,
    };
}

/// Draws the game into the bound framebuffer, as `view` shows it, with the crosshair if the view has it, the
/// portraits of the party, the spells in effect and the bar of mana.
///
/// \param renderer         Draws the world.
/// \param overlay_renderer Draws the crosshair.
/// \param portraits        Draws the portraits of the party.
/// \param game             What to draw.
/// \param view             How to show it.
/// \param framebuffer      Size of the bound framebuffer. The picture is centered in it, see fit_viewport().
void draw_game(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
               const PartyPortraits& portraits, const manifold::Game& game, const manifold::ViewSettings& view,
               const manifold::PixelSize& framebuffer) {
    // Candlelight at the player's eyes while it is in effect, the level's ambient light if any, and the lights of the
    // torches and the exit nearest to the camera.
    const manifold::Camera camera = manifold::view_camera(view, game.player());
    const std::optional<glm::vec3> party_light =
        game.magic().active(manifold::Spell::kCandlelight) ? std::optional(game.player().eye_position()) : std::nullopt;
    manifold::Lighting lighting = manifold::scene_lighting(view.lights, party_light, game.world().torches(),
                                                           game.world().exit(), game.animation_time(), camera.position);
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
    portraits.draw(game.party(), overlay_renderer, viewport);
    manifold::OverlayMesh magic;
    manifold::add_spell_hud(magic, {.width = viewport.width, .height = viewport.height}, game.magic());
    overlay_renderer.draw(magic, viewport);
}

/// Draws the game into the bound framebuffer, as draw_game() does, with the open screens of the interface over it, or
/// only the screens if one of them hides the game.
void draw_scene(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
                const PartyPortraits& portraits, const manifold::Game& game, const manifold::ViewSettings& view,
                const manifold::ScreenStack& screens, const manifold::PixelSize& framebuffer) {
    if (screens.hides_game()) {
        manifold::clear_to_black();
    } else {
        draw_game(renderer, overlay_renderer, portraits, game, view, framebuffer);
    }
    if (!screens.empty()) {
        const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
        overlay_renderer.draw(screens.build_mesh({.width = viewport.width, .height = viewport.height}), viewport);
    }
}

/// Takes the screenshots that console commands asked for, each drawn as it asked, with the open screens of the
/// interface, see draw_scene().
///
/// A screenshot that fails is logged, and stops the game only if its request says so.
///
/// \param window_size The size of screenshots that don't give their own.
/// \throws std::runtime_error if a screenshot that stops on error fails.
void take_screenshots(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
                      const PartyPortraits& portraits, const manifold::Game& game, const manifold::ScreenStack& screens,
                      std::span<const manifold::ScreenshotRequest> requests, const manifold::PixelSize& window_size) {
    for (const manifold::ScreenshotRequest& request : requests) {
        const manifold::PixelSize size = request.size.value_or(window_size);
        try {
            const manifold::OffscreenFramebuffer framebuffer(size);
            draw_scene(renderer, overlay_renderer, portraits, game, request.view, screens, size);
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

/// Opens the menu that `request` asks for instead of the open screens, or closes them all.
///
/// \param can_load Whether there is a save to load, which the menus offer only then.
void open_menu(manifold::MenuRequest request, manifold::ScreenStack& screens, bool can_load) {
    screens.clear();
    switch (request) {
        case manifold::MenuRequest::kStart:
            screens.push(manifold::make_start_menu(can_load));
            break;
        case manifold::MenuRequest::kGame:
            screens.push(manifold::make_game_menu(can_load));
            break;
        case manifold::MenuRequest::kClose:
            break;
    }
}

/// Does what a screen of the interface asked for. Loading or starting a new game closes all screens, unless it fails,
/// which is logged.
///
/// \param layout The layout of the first level of a new game, if given on the command line.
void apply_ui_command(manifold::UiCommand command, manifold::ScreenStack& screens, LevelSession& level,
                      manifold::Game& game, manifold::ConsoleState& state, std::optional<manifold::LayoutKind> layout) {
    switch (command) {
        case manifold::UiCommand::kResume:
            screens.pop();
            break;
        case manifold::UiCommand::kNewGame:
            if (level.new_game(game, state.view, layout)) {
                screens.clear();
            }
            break;
        case manifold::UiCommand::kLoad:
            if (level.load(game, state.view, false)) {
                screens.clear();
            }
            break;
        case manifold::UiCommand::kExit:
            state.quit = true;
            break;
    }
}

/// Does what was asked for since the last frame, at the start of a frame: what a screen of the interface asked for,
/// the menu that Escape or the `menu` command asked for, and loading. Saves the new game that the start menu offered
/// once no screen is open.
///
/// \param layout The layout of the first level of a new game, if given on the command line.
/// \throws std::runtime_error if the `load` command asked to load and to stop on error, and loading fails.
void apply_requests(Ui& ui, manifold::ConsoleState& state, LevelSession& level, manifold::Game& game,
                    std::optional<manifold::LayoutKind> layout) {
    if (const std::optional<manifold::UiCommand> command = std::exchange(ui.command, std::nullopt)) {
        apply_ui_command(*command, ui.screens, level, game, state, layout);
    }
    if (const std::optional<manifold::MenuRequest> menu = std::exchange(state.menu, std::nullopt)) {
        open_menu(*menu, ui.screens, level.has_save());
    }
    // Loading comes before the commands, so that those after `load` run in the loaded level. As from a menu, loading
    // closes the menus.
    if (std::exchange(state.load, false) && level.load(game, state.view, state.load_stops_on_error)) {
        ui.screens.clear();
    }
    // The new game is saved once the start menu is closed without continuing the save, also by the `menu` command.
    if (ui.screens.empty()) {
        level.save_deferred(game);
    }
}

/// Runs the game until the player quits or a console command does.
///
/// \throws manifold::CommandError if one of the commands of `options` fails.
/// \throws std::runtime_error if a level can't be generated, also the next one when the player reaches the exit, or if
///         a `load` among the commands of `options` fails.
void run(const Options& options) {
    // The console window shows the log, from the start.
    manifold::ConsoleWindow console_window;
    manifold::set_log_listener([&console_window](manifold::LogLevel level, std::string_view message) {
        console_window.add_output(level, message);
    });
    const LogListenerReset reset_log_listener;

    const AssetDirs assets = find_asset_dirs();
    manifold::GameConfig config;
    config.portraits = manifold::find_portraits(assets.portraits);
    manifold::log_info("Portraits: {} in {}", config.portraits.size(), assets.portraits.string());
    manifold::Game game(config, options.seed.value_or(random_seed()), options.layout);

    const std::optional<std::filesystem::path> data_dir = user_data_dir();
    // Scripts of console commands mostly test the game and take screenshots, so they run in a window, which doesn't
    // take over the screen.
    const bool scripted = !options.commands.empty();
    const bool fullscreen = read_settings(data_dir).fullscreen && !scripted;
    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height, fullscreen);
    manifold::ConsoleState state;
    manifold::AudioOutput audio;
    const DoorSoundSets door_sounds(assets.sounds);
    FootstepSounds footsteps(assets.sounds);
    const manifold::OverlayRenderer overlay_renderer;
    PartyPortraits portraits(assets.portraits);
    // Each level is saved when it starts, which replaces the save of the level before. Scripts of console commands
    // mostly test the game, so they leave the player's save alone. If there is a save, the player is asked first
    // whether to continue it, and the new game is saved only once the start menu is closed without continuing.
    const bool saving = !scripted;
    std::optional<std::filesystem::path> save_path = save_file_path(data_dir);
    const bool ask = saving && has_readable_save(save_path);
    LevelSession level(game, state.view, assets, std::move(save_path), saving, ask);
    Ui ui;
    if (ask) {
        ui.screens.push(manifold::make_start_menu(true));
    }

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
        apply_requests(ui, state, level, game, options.layout);
        for (const std::string& line : terminal.read_lines()) {
            commands.add(line, false);
        }
        commands.run_frame(console);

        const std::uint64_t ticks = SDL_GetTicksNS();
        const float frame_time = static_cast<float>(ticks - previous_ticks) * 1e-9f;
        previous_ticks = ticks;
        frame_times.add(frame_time);
        // The game is paused while a screen such as a menu is open.
        if (ui.screens.empty()) {
            // While the console window is open, or the debug overlay has the keyboard, the keys type into them
            // instead of moving the player, and clicks don't activate.
            const bool keys_taken = console_window.is_open() || overlay.wants_keyboard();
            manifold::PlayerInput input =
                keys_taken ? manifold::PlayerInput{} : read_input(ui, state.view, game.player());
            if (state.walk_frames > 0) {
                input.forward = 1.0f;
                input.run = input.run || state.walk_runs;
                --state.walk_frames;
            }
            game.update(input, std::min(frame_time, kMaxFrameTime));
        }
        ui.game_click.reset();
        ui.casts.clear();
        // The player reached the exit, or a console command started the next level as if they had.
        level.follow(game, state.view, audio);
        play_door_sounds(game, door_sounds, audio);
        footsteps.play(game, audio);
        // A new game or a loaded one has another party.
        portraits.follow(game.party());

        const manifold::PixelSize window_size = window.framebuffer_size();
        const manifold::Viewport viewport = manifold::fit_viewport(window_size, manifold::kAspectRatio);
        ui.picture = {.width = viewport.width, .height = viewport.height};
        draw_scene(level.renderer(), overlay_renderer, portraits, game, state.view, ui.screens, window_size);
        // The stats of this draw, before screenshots draw again.
        const manifold::RenderStats render_stats = level.renderer().stats();
        // A screen that hides the game hides the overlay too, whose numbers would be of the last drawing of the game.
        if (state.debug_overlay && !ui.screens.hides_game()) {
            overlay.draw(game, state.view, render_stats, frame_times);
        }
        if (console_window.is_open()) {
            overlay_renderer.draw(console_window.build_mesh(ui.picture), viewport);
        }
        take_screenshots(level.renderer(), overlay_renderer, portraits, game, ui.screens, state.screenshots,
                         window_size);
        state.screenshots.clear();
        if (state.quit) {
            return;
        }
        window.swap_buffers();
        // The console window has the keys while it is open, and the screens have the keys and the mouse, so the
        // overlay gets no events then.
        overlay.set_input(state.debug_overlay && !console_window.is_open() && ui.screens.empty());
    } while (handle_events(window, console_window, commands, overlay, state, ui, console_key_held, game));
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
