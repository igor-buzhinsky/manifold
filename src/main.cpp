#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
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
#include "audio/spell_sounds.hpp"
#include "console/console.hpp"
#include "console/console_window.hpp"
#include "console/game_commands.hpp"
#include "data/game_data.hpp"
#include "game.hpp"
#include "items/item_catalog.hpp"
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
#include "ui/damage_flash.hpp"
#include "ui/debug_overlay.hpp"
#include "ui/game_menus.hpp"
#include "ui/inventory_screen.hpp"
#include "ui/mouse_look.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/party_hud.hpp"
#include "ui/screen.hpp"
#include "ui/screen_stack.hpp"
#include "ui/spell_hud.hpp"
#include "ui/text_field.hpp"
#include "ui/ui_event.hpp"
#include "util/asset_files.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/frame_times.hpp"
#include "util/log.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"
#include "world/door.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

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

/// What the game reads from the directories of its assets and its data.
struct GameAssets {
    manifold::AssetMapping mapping;  ///< Which files the game uses for what.
    manifold::GameData data;         ///< The settings of the game, with all kinds of items.
};

/// Finds the game's assets and data, see manifold::find_game_dir(): the `assets` and `data` directories next to the
/// executable, or else those in the source tree, whose paths CMake sets, and reads the mapping of the assets, see
/// manifold::AssetMapping::load(), and the data, see manifold::load_game_data(). Logs which directories are used.
///
/// \throws manifold::GameDataError if the data files are missing or wrong.
GameAssets load_assets() {
    // SDL keeps the string, so it isn't freed.
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        manifold::log_warning("The directory of the executable isn't known: {}", SDL_GetError());
    }
    const std::filesystem::path executable_dir =
        base_path != nullptr ? std::filesystem::path(base_path) : std::filesystem::path();
    const std::filesystem::path assets = manifold::find_assets_dir(executable_dir, MANIFOLD_ASSETS_DIR);
    const std::filesystem::path data =
        manifold::find_game_dir(executable_dir, manifold::kDataDirName, MANIFOLD_DATA_DIR);
    manifold::log_info("Assets: {}, data: {}", assets.string(), data.string());
    return {.mapping = manifold::AssetMapping::load(assets), .data = manifold::load_game_data(data)};
}

/// Loads the texture of the game's current level, see manifold::Level::texture. If it can't be used, a plain grey one
/// is used instead, after a warning. Which file is used is logged as info.
manifold::Image load_texture_of_level(const manifold::Game& game, const manifold::AssetMapping& assets) {
    const manifold::LevelTexture& texture = game.level().texture;
    const manifold::AssetKey key = manifold::level_textures_key(texture.kind);
    const std::optional<std::filesystem::path> path = assets.path(key, texture.index);
    std::optional<manifold::Image> image = path ? manifold::load_level_texture(*path) : std::nullopt;
    if (!image) {
        manifold::log_warning("Texture {} {} can't be used, so the level is plain grey", manifold::asset_key_name(key),
                              texture.index);
        return manifold::fallback_texture();
    }
    manifold::log_info("Texture: {} ({}x{}), kind {}", path->string(), image->width, image->height,
                       manifold::texture_kind_name(texture.kind));
    return std::move(*image);
}

/// The sounds of doors and chests, loaded once for all levels.
struct ObjectSounds {
    explicit ObjectSounds(const manifold::AssetMapping& assets)
        : door_open(assets, manifold::AssetKey::kDoorOpenSounds),
          door_close(assets, manifold::AssetKey::kDoorCloseSounds),
          chest_open(assets, manifold::AssetKey::kChestOpenSounds) {}

    manifold::SoundSet door_open;
    manifold::SoundSet door_close;
    manifold::SoundSet chest_open;
};

/// Plays the sounds of the doors that started to move since the last call, see manifold::Game::take_door_moves(), and
/// of the chests that were activated, see manifold::Game::take_chest_activations(). Each door and chest has its own
/// sounds, see manifold::DoorSounds and manifold::Chest::open_sound.
///
/// \param chests The indices of the chests that were activated, into manifold::World::chests().
void play_object_sounds(manifold::Game& game, std::span<const std::size_t> chests, const ObjectSounds& sounds,
                        manifold::AudioOutput& audio) {
    for (const manifold::DoorMove& move : game.take_door_moves()) {
        const manifold::DoorSounds& door = game.world().doors()[move.door].sounds;
        const manifold::Sound* sound = move.open ? sounds.door_open.get(door.open) : sounds.door_close.get(door.close);
        if (sound != nullptr) {
            audio.play(*sound);
        }
    }
    for (const std::size_t chest : chests) {
        if (const manifold::Sound* sound = sounds.chest_open.get(game.world().chests()[chest].open_sound)) {
            audio.play(*sound);
        }
    }
}

/// Plays the sounds of the spells that were cast since the last call, see manifold::Game::take_casts().
void play_spell_sounds(manifold::Game& game, const manifold::SpellSounds& sounds, manifold::AudioOutput& audio) {
    for (const manifold::Spell spell : game.take_casts()) {
        if (const manifold::Sound* sound = sounds.get(spell)) {
            audio.play(*sound);
        }
    }
}

/// How loud footsteps are played, relative to the files: they are scaled to the same peak as the other sounds, but
/// come often and from close by.
constexpr float kFootstepGain = 0.5f;

/// The footstep sounds of each texture kind, loaded once for all levels, and which was played last.
class FootstepSounds {
public:
    explicit FootstepSounds(const manifold::AssetMapping& assets) : random_(random_seed()) {
        for (const manifold::TextureKind kind : manifold::kTextureKinds) {
            sets_.at(static_cast<std::size_t>(kind)) = manifold::SoundSet(assets, manifold::footstep_sounds_key(kind));
        }
    }

    /// Plays the footsteps that the player made since the last call, see manifold::Game::take_footsteps(), with the
    /// sounds of the level's texture kind. Two steps in a row sound different, if the kind has more than one sound.
    void play(manifold::Game& game, manifold::AudioOutput& audio) {
        const int steps = game.take_footsteps();
        const manifold::SoundSet& set = sets_.at(static_cast<std::size_t>(game.level().texture.kind));
        for (int step = 0; step < steps; ++step) {
            previous_ = set.pick_other(previous_, random_());
            if (const manifold::Sound* sound = set.get(previous_)) {
                audio.play(*sound, kFootstepGain);
            }
        }
    }

private:
    std::array<manifold::SoundSet, manifold::kTextureKinds.size()> sets_;
    manifold::AssetIndex previous_ = 0;  ///< The index of the footstep played last, see manifold::SoundSet::get().
    manifold::Rng random_;               ///< Picks the footsteps. Unlike the level, it doesn't depend on the seed.
};

/// Makes the renderer of the game's current level and sets the level's ambient light in `view`.
void start_level(std::optional<manifold::Renderer>& renderer, const manifold::Game& game, manifold::ViewSettings& view,
                 const manifold::AssetMapping& assets) {
    const manifold::Image texture = load_texture_of_level(game, assets);
    view.level_ambient =
        manifold::level_ambient(game.seed(), manifold::mean_linear_color(texture), view.lights.level_ambient_luminance);
    view.lights.ambient = view.level_ambient;
    renderer.emplace(game.world(), texture);
}

/// The images of the portraits of the party, which are loaded again whenever the party gets other portraits, and draws
/// them with the bars of hit points, see manifold::layout_party_hud().
///
/// Needs a current OpenGL 4.1 core context, see manifold::Window for its whole lifetime.
class PartyPortraits {
public:
    /// \param assets Where the files of the portraits are. It must outlive the object.
    explicit PartyPortraits(const manifold::AssetMapping& assets) : assets_(&assets) {}

    /// Loads the images of the portraits of `party` if they are not those loaded last. A portrait that can't be loaded
    /// is logged as a warning and shows as an empty place, as does a character without one.
    void follow(std::span<const manifold::Character> party) {
        std::vector<std::optional<manifold::Portrait>> portraits;
        portraits.reserve(party.size());
        for (const manifold::Character& character : party) {
            portraits.push_back(character.portrait);
        }
        if (portraits == portraits_) {
            return;
        }
        images_.clear();
        for (const std::optional<manifold::Portrait>& portrait : portraits) {
            std::optional<manifold::ImageTexture>& texture = images_.emplace_back();
            if (!portrait) {
                continue;
            }
            const std::optional<std::filesystem::path> path = assets_->path(portrait->key, portrait->index);
            if (const std::optional<manifold::Image> image = path ? manifold::load_image(*path) : std::nullopt) {
                texture.emplace(*image);
            }
        }
        portraits_ = std::move(portraits);
    }

    /// Draws the portraits of `party`, which follow() was called for, and their bars into `viewport`. The portraits of
    /// dead characters are gray.
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
                image_renderer_.draw(*image, placements[i].portrait, viewport, party[i].dead());
            }
        }
    }

private:
    const manifold::AssetMapping* assets_;
    manifold::ImageRenderer image_renderer_;
    /// manifold::Character::portrait of each character loaded last.
    std::vector<std::optional<manifold::Portrait>> portraits_;
    /// The image of each character loaded last, or nothing if it has none.
    std::vector<std::optional<manifold::ImageTexture>> images_;
};

/// The icons of all kinds of items, loaded once, which draws those that the screens of the interface show.
///
/// Needs a current OpenGL 4.1 core context, see manifold::Window for its whole lifetime.
class ItemIcons {
public:
    /// Loads the icon of each item of `items`, see manifold::AssetMapping::item_icon(). An item without one, or whose
    /// icon can't be loaded, shows none, after a warning.
    ItemIcons(const manifold::AssetMapping& assets, const manifold::ItemCatalog& items) {
        for (const manifold::ItemType& item : items.items()) {
            std::optional<manifold::ImageTexture>& texture = textures_.emplace_back();
            const std::optional<std::filesystem::path> path = assets.item_icon(item.key);
            if (!path) {
                manifold::log_warning("Item {} has no icon in {}", item.key, manifold::kItemIconsKey);
                continue;
            }
            if (const std::optional<manifold::Image> image = manifold::load_image(*path)) {
                texture.emplace(*image);
            }
        }
    }

    /// Draws `icons` into `viewport`, over what is there.
    void draw(std::span<const manifold::IconPlacement> icons, const manifold::Viewport& viewport) const {
        for (const manifold::IconPlacement& icon : icons) {
            if (icon.item >= textures_.size()) {
                continue;
            }
            if (const std::optional<manifold::ImageTexture>& texture = textures_[icon.item]) {
                image_renderer_.draw(*texture, icon.area, viewport);
            }
        }
    }

private:
    manifold::ImageRenderer image_renderer_;
    /// The icon of each item, by manifold::ItemId, or nothing if it has none.
    std::vector<std::optional<manifold::ImageTexture>> textures_;
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

/// Where the two saves are, see manifold::SaveKind: in the game's directory for user data, see user_data_dir().
struct SavePaths {
    std::filesystem::path progress;     ///< `save.json`.
    std::filesystem::path level_start;  ///< `save_level_start.json`.

    /// The path of the save of `kind`.
    const std::filesystem::path& of(manifold::SaveKind kind) const {
        return kind == manifold::SaveKind::kProgress ? progress : level_start;
    }
};

/// Where the game is saved, see SavePaths.
///
/// \return The paths, or nothing (after logging a warning) if there is no directory for user data.
std::optional<SavePaths> save_file_paths(const std::optional<std::filesystem::path>& data_dir) {
    if (!data_dir) {
        manifold::log_warning("The game can't be saved or loaded");
        return std::nullopt;
    }
    return SavePaths{.progress = *data_dir / manifold::save_file_name(manifold::SaveKind::kProgress),
                     .level_start = *data_dir / manifold::save_file_name(manifold::SaveKind::kLevelStart)};
}

/// The autosave interval of `settings` for LevelSession, in seconds, or nothing if autosave is off.
std::optional<double> autosave_interval(const manifold::Settings& settings) {
    if (!settings.autosave) {
        return std::nullopt;
    }
    return settings.autosave_interval;
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

/// How write_save() logs.
struct SaveLogging {
    bool success = true;  ///< Whether a save that succeeds is logged. Autosaves aren't, since they would fill the log.
    bool failure = true;  ///< Whether a save that fails is logged.
};

/// Writes a save to `path`, see manifold::write_save_file(). A failure is only logged.
///
/// \return Whether the save was written.
bool write_save(const manifold::SavedGame& save, const std::filesystem::path& path, SaveLogging logging = {}) {
    try {
        manifold::write_save_file(path, save);
        if (logging.success) {
            manifold::log_info("Saved the game to {}", path.string());
        }
        return true;
    } catch (const std::runtime_error& error) {
        if (logging.failure) {
            manifold::log_error("Can't save the game: {}", error.what());
        }
        return false;
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
bool has_readable_save(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return false;
    }
    try {
        manifold::read_save_file(path);
        return true;
    } catch (const std::runtime_error& read_error) {
        manifold::log_warning("The saved game can't be read, so it can't be continued: {}", read_error.what());
        return false;
    }
}

/// Which saves there are that can be read, see has_readable_save(), to offer to continue them when the game starts.
///
/// \param paths  Where the saves are, or nothing if there is no directory for saves, which has none.
/// \param saving Whether this run saves the game. If not, no save is offered, since the run doesn't play it.
manifold::SavesToLoad saves_to_continue(const std::optional<SavePaths>& paths, bool saving) {
    if (!paths || !saving) {
        return {};
    }
    return {.progress = has_readable_save(paths->progress), .level_start = has_readable_save(paths->level_start)};
}

/// What belongs to the game's current level outside the game: its renderer and the sound of its exit. If saving is on,
/// each level that starts is saved, to both saves, and the game is saved to `save.json` every autosave interval and by
/// save_on_exit(), but neither save is written while the whole party is dead, and `save.json` is deleted then. The
/// saved game can be loaded either way.
class LevelSession {
public:
    /// Sets up the game's current level.
    ///
    /// \param game              The game, whose level is set up.
    /// \param view              Where the level's ambient light is set.
    /// \param assets            Where the files of the level's texture and exit sound are. It must outlive the object.
    /// \param save_paths        Where the game is saved, or nothing if there is no directory for saves.
    /// \param saving            Whether to save the game. If not, the saves can still be loaded.
    /// \param autosave_interval Seconds of game time between saves to `save.json`, or nothing to save it only when a
    ///                          level starts and on exit.
    /// \param defer             Whether to save the game's level only when save_deferred() is called, e.g. after the
    ///                          player chose not to continue the save that is there, instead of now.
    LevelSession(manifold::Game& game, manifold::ViewSettings& view, const manifold::AssetMapping& assets,
                 std::optional<SavePaths> save_paths, bool saving, std::optional<double> autosave_interval, bool defer)
        : assets_(&assets),
          exit_sounds_(assets, manifold::AssetKey::kExitSounds),
          save_paths_(std::move(save_paths)),
          saving_(saving),
          autosave_interval_(autosave_interval) {
        if (save_paths_ && !saving_) {
            manifold::log_info(
                "The game isn't saved in this run, since it runs console commands from the command line");
        }
        set_up(game, view, defer ? Saving::kDeferred : Saving::kNow);
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
            if (const manifold::Sound* sound = exit_sounds_.get(*exit_sound_)) {
                audio.play(*sound);
            }
        }
        set_up(game, view, deferred_ ? Saving::kDeferred : Saving::kNow);
    }

    /// Loads a save into `game` and sets up its level. Unless `stop_on_error` is set, a failure is only logged, and the
    /// level stays.
    ///
    /// Loading `save.json` saves the game to both saves, see set_up(). Loading the save of the level's start writes
    /// nothing: `save.json` is replaced only by the next autosave, an autosave interval of game time later, or on
    /// exit, so that until then, loading it undoes a restart chosen by mistake.
    ///
    /// \return Whether the game was loaded.
    /// \throws std::runtime_error if loading fails and `stop_on_error` is set.
    bool load(manifold::Game& game, manifold::ViewSettings& view, manifold::SaveKind kind, bool stop_on_error) {
        const std::optional<std::filesystem::path> path =
            save_paths_ ? std::optional(save_paths_->of(kind)) : std::nullopt;
        if (!load_game(game, path, stop_on_error)) {
            return false;
        }
        if (kind == manifold::SaveKind::kLevelStart) {
            set_up(game, view, Saving::kNone);
            last_save_time_ = game.time();
        } else {
            set_up(game, view);
        }
        return true;
    }

    /// Starts a new game, which replaces the saves, with a random seed and `layout` for its first level, see
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
            save_level_start(game);
        }
    }

    /// Saves the game to `save.json` if the autosave interval of game time has passed since it was last saved there,
    /// unless the level waits for save_deferred().
    void autosave(const manifold::Game& game) {
        if (autosave_interval_ && !deferred_ && game.time() - last_save_time_ >= *autosave_interval_) {
            save_progress(game, true);
        }
    }

    /// Saves the game to `save.json` when the game ends, unless the level waits for save_deferred(): then the player
    /// hasn't chosen yet whether to continue the save, which stays.
    void save_on_exit(const manifold::Game& game) {
        if (!deferred_) {
            save_progress(game);
        }
    }

    /// Deletes `save.json`, if saving is on, once the whole party is dead, so that neither Load game nor Continue can
    /// go back to just before the death; only the start of the level can be loaded then. A failure is logged as a
    /// warning.
    void discard_progress() {
        if (!saving_ || !save_paths_) {
            return;
        }
        std::error_code error;
        if (std::filesystem::remove(save_paths_->progress, error)) {
            manifold::log_info("Deleted {}, since the party is dead", save_paths_->progress.string());
        } else if (error) {
            manifold::log_warning("Can't delete {}: {}", save_paths_->progress.string(), error.message());
        }
    }

    /// Which saves there are to load. They may still fail to load.
    manifold::SavesToLoad saves() const {
        if (!save_paths_) {
            return {};
        }
        std::error_code error;
        const bool progress = std::filesystem::exists(save_paths_->progress, error);
        return {.progress = progress, .level_start = std::filesystem::exists(save_paths_->level_start, error)};
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
    /// When set_up() saves the level that it sets up.
    enum class Saving : std::uint8_t {
        kNow,       ///< Now, to both saves.
        kDeferred,  ///< To both saves when save_deferred() is called. Until then, nothing is saved.
        /// Not now: `save.json` stays until the next autosave, the next level or the exit, and `save_level_start.json`
        /// until the next level.
        kNone,
    };

    /// Logs the game's current level, makes its renderer, see start_level(), notes its exit sound, and saves the
    /// game as `saving` says, if saving is on.
    void set_up(manifold::Game& game, manifold::ViewSettings& view, Saving saving = Saving::kNow) {
        log_game(game);
        start_level(renderer_, game, view, *assets_);
        const std::optional<manifold::LevelExit>& exit = game.world().exit();
        exit_sound_ = exit ? std::optional(exit->sound) : std::nullopt;
        level_starts_ = game.level_starts();
        deferred_ = saving == Saving::kDeferred;
        if (saving == Saving::kNow) {
            save_level_start(game);
        }
    }

    /// Saves the start of the game's level to `save_level_start.json`, and the game as it is to `save.json`, if saving
    /// is on, unless the whole party is dead, e.g. after `next_level` when the `hp` cheat killed it, see
    /// save_progress().
    void save_level_start(const manifold::Game& game) {
        if (saving_ && save_paths_ && !game.defeated()) {
            write_save(game.level_start_save(), save_paths_->level_start);
        }
        save_progress(game);
    }

    /// Saves the game as it is to `save.json`, if saving is on, unless the whole party is dead: then `save.json` is
    /// deleted, see discard_progress(), and stays so until the level is restarted or a new game starts.
    ///
    /// \param autosave Whether it is an autosave, which isn't logged if it succeeds, nor if it fails after the save to
    ///                 `save.json` before it failed too, so that a directory that can't be written doesn't fill the
    ///                 log with an error every autosave interval.
    void save_progress(const manifold::Game& game, bool autosave = false) {
        if (saving_ && save_paths_ && !game.defeated()) {
            const bool saved = write_save(game.progress_save(), save_paths_->progress,
                                          {.success = !autosave, .failure = !autosave || !progress_save_failed_});
            if (!saved && autosave_interval_ && !progress_save_failed_) {
                manifold::log_warning("Autosaves that fail aren't logged until one succeeds");
            }
            progress_save_failed_ = !saved;
        }
        last_save_time_ = game.time();
    }

    const manifold::AssetMapping* assets_;
    manifold::SoundSet exit_sounds_;  ///< The sounds of exits, loaded once for all levels.
    std::optional<SavePaths> save_paths_;
    bool saving_;                              ///< Whether the game is saved, if there is a directory for saves.
    std::optional<double> autosave_interval_;  ///< See the constructor.
    /// manifold::Game::time() that the next autosave counts its interval from: when `save.json` was last saved, or
    /// would have been, or when the start of the level was loaded, see load().
    double last_save_time_ = 0.0;
    bool progress_save_failed_ = false;           ///< Whether the last save to `save.json` failed, see save_progress().
    std::optional<manifold::Renderer> renderer_;  ///< The renderer has the meshes of one level.
    std::optional<manifold::AssetIndex> exit_sound_;  ///< The sound of the level's exit, if it has one.
    int level_starts_ = 0;                            ///< manifold::Game::level_starts() of the level that is set up.
    bool deferred_ = false;  ///< Whether the level that is set up waits for save_deferred() to be saved.
};

/// The key that opens and closes the console: the one left of 1, labelled `/~ on US keyboards.
constexpr SDL_Scancode kConsoleKey = SDL_SCANCODE_GRAVE;
/// The key that shows and hides the debug overlay.
constexpr SDL_Keycode kDebugOverlayKey = SDLK_F3;
/// The key that opens and closes the party's inventory. Like the movement keys, it is the key at the place of I on US
/// keyboards.
constexpr SDL_Scancode kInventoryKey = SDL_SCANCODE_I;
/// The key that takes all items of an open chest, at the place of T on US keyboards.
constexpr SDL_Scancode kTakeAllKey = SDL_SCANCODE_T;
/// The key that loads the saved game as it was at the last autosave or exit, see manifold::SaveKind::kProgress.
constexpr SDL_Keycode kLoadKey = SDLK_F9;
/// The keys that cast spells. Like the movement keys, they are the keys at the places of C and L on US keyboards.
constexpr SDL_Scancode kCandlelightKey = SDL_SCANCODE_C;
constexpr SDL_Scancode kLevitateKey = SDL_SCANCODE_L;
/// The key that turns mouse look on and off, at the place of M on US keyboards.
constexpr SDL_Scancode kMouseLookKey = SDL_SCANCODE_M;

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
    /// are left out. While mouse look is active, presses go to `center_click` instead.
    std::optional<glm::vec2> game_click;
    /// When the left button was last pressed over the game in this frame while mouse look was active, in nanoseconds
    /// as SDL gives the times of events. It activates at the center of the view, as E does, unless it is the click
    /// that gave the window the focus, see manifold::is_focus_click().
    std::optional<std::uint64_t> center_click;
    /// When the window last got the focus, in the same nanoseconds, if it did.
    std::optional<std::uint64_t> focus_time;
    /// Whether the player wants mouse look, from the settings and kMouseLookKey.
    bool mouse_look = false;
    /// Whether mouse look is active: wanted, and not paused by a screen, a menu asked for, the console window or the
    /// debug overlay, which need the pointer. Relative mouse mode is on exactly then, see update_mouse_look().
    bool mouse_look_active = false;
    /// How far mouse look turns the view, in degrees per unit of mouse motion, see Settings::mouse_sensitivity.
    float mouse_sensitivity = manifold::Settings{}.mouse_sensitivity;
    /// The motion of the mouse in this frame while mouse look was active, x right and y down.
    glm::vec2 mouse_motion{0.0f};
    /// The spells whose keys were pressed in this frame while no screen was open, in order.
    std::vector<manifold::Spell> casts;
    /// manifold::Game::level_starts() of the level whose things the screens may show, see close_old_level_screens().
    int level_starts = 0;

    /// Passes an event to the screen on top, unless the screens already asked for something in this frame.
    void handle(const manifold::UiEvent& event) {
        if (!command) {
            command = screens.handle(event, picture);
        }
    }
};

/// The key of the interface's screens that `key` is, if any, besides Escape, which is handled on its own.
std::optional<manifold::NavKey> nav_key(const SDL_KeyboardEvent& key) {
    if (key.scancode == kInventoryKey) {
        return manifold::NavKey::kInventory;
    }
    if (key.scancode == kTakeAllKey) {
        return manifold::NavKey::kTakeAll;
    }
    switch (key.key) {
        case SDLK_UP:
            return manifold::NavKey::kUp;
        case SDLK_DOWN:
            return manifold::NavKey::kDown;
        case SDLK_LEFT:
            return manifold::NavKey::kLeft;
        case SDLK_RIGHT:
            return manifold::NavKey::kRight;
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
/// and otherwise the load key, which asks `state` to load the game, the inventory key, which asks `state` to show the
/// party's inventory, the mouse look key, which turns `ui.mouse_look` on or off, and the keys of spells, which are kept
/// in `ui.casts`.
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
        // Holding an arrow key moves on, but holding Enter doesn't choose again, nor do the keys of the inventory.
        const std::optional<manifold::NavKey> nav = nav_key(key);
        const bool moves = nav == manifold::NavKey::kUp || nav == manifold::NavKey::kDown ||
                           nav == manifold::NavKey::kLeft || nav == manifold::NavKey::kRight;
        if (nav && (!key.repeat || moves)) {
            ui.handle(*nav);
        }
    } else if (key.repeat) {
        return;
    } else if (key.key == kLoadKey) {
        state.load = manifold::SaveKind::kProgress;
        state.load_stops_on_error = false;
    } else if (key.scancode == kInventoryKey) {
        state.inventory = manifold::InventoryRequest{};
    } else if (key.scancode == kMouseLookKey) {
        ui.mouse_look = !ui.mouse_look;
        manifold::log_info("Mouse look {}", ui.mouse_look ? "on" : "off");
    } else if (const std::optional<manifold::Spell> spell = spell_key(key.scancode)) {
        ui.casts.push_back(*spell);
    }
}

/// The point of the picture under a point of the window, in pixels from its top-left corner, y down: the center of the
/// pixel under it, see manifold::pixel_center().
///
/// The picture is laid out for the current size of the window, not that of the last drawing, in case the window was
/// resized. Also updates `ui.picture` to its size.
glm::vec2 picture_point(const glm::vec2& point, const manifold::Window& window, Ui& ui) {
    const manifold::PixelSize framebuffer = window.framebuffer_size();
    const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
    ui.picture = {.width = viewport.width, .height = viewport.height};
    // As drawn, a pixel belongs to the shape that its center is in, also where edges fall between pixels.
    return manifold::pixel_center(manifold::window_to_viewport(point, window.size(), framebuffer, viewport));
}

/// Passes a mouse event to the screen on top as a pointer event, in pixels of the picture, if it is a motion or the
/// left button and a screen is open, or as a scroll event if it is a turn of the wheel. Without a screen, a press of
/// the left button over the picture is kept in `ui.game_click`, to activate what is there, unless it is on the
/// portraits of the party, the spells in effect or the bar of mana, see manifold::party_hud_contains() and
/// manifold::spell_hud_contains(). While mouse look is active, motion is added to `ui.mouse_motion` instead, and a
/// press is kept in `ui.center_click`.
///
/// \param game The game, whose party and magic are shown over it.
void handle_mouse_event(const SDL_Event& event, const manifold::Window& window, Ui& ui, const manifold::Game& game) {
    if (ui.screens.empty() && ui.mouse_look_active) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            ui.mouse_motion += glm::vec2(event.motion.xrel, event.motion.yrel);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            ui.center_click = event.button.timestamp;
        }
        return;
    }
    if (ui.screens.empty()) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            const glm::vec2 point = picture_point({event.button.x, event.button.y}, window, ui);
            const glm::vec2 size{static_cast<float>(ui.picture.width), static_cast<float>(ui.picture.height)};
            if (point.x >= 0.0f && point.y >= 0.0f && point.x < size.x && point.y < size.y &&
                !manifold::party_hud_contains(ui.picture, game.party().size(), point) &&
                !manifold::spell_hud_contains(ui.picture, game.magic(), game.magic_config(), point)) {
                ui.game_click = point / size;
            }
        }
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        const float flip = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
        ui.handle(
            manifold::ScrollEvent{.steps = event.wheel.y * flip,
                                  .position = picture_point({event.wheel.mouse_x, event.wheel.mouse_y}, window, ui)});
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
                                         .position = {event.button.x, event.button.y},
                                         .clicks = event.button.clicks};
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

/// Makes mouse look active if the player wants it and nothing needs the mouse pointer: no screen of the interface is
/// open or asked for, nor the console window or the debug overlay. Turns relative mouse mode on or off when that
/// changes.
void update_mouse_look(Ui& ui, const manifold::ConsoleWindow& console_window, const manifold::ConsoleState& state,
                       manifold::Window& window) {
    const bool active =
        ui.mouse_look && ui.screens.empty() && !state.menu && !console_window.is_open() && !state.debug_overlay;
    if (active != ui.mouse_look_active) {
        ui.mouse_look_active = active;
        window.set_relative_mouse_mode(active);
    }
}

/// Handles pending window, key and mouse events: quitting, the console window, the debug overlay, the screens of the
/// interface, loading, casting spells and mouse look, which is updated after each event, see update_mouse_look(), so
/// that the events after one that pauses it or turns it on are handled in the new way.
///
/// These are key bindings, like those of read_input(). Keep GAME_MANUAL.md up to date when changing them, and
/// DEBUGGING.md for the keys of the console window.
///
/// \param window             The window, whose text input is on while the console window is open, and relative mouse
///                           mode while mouse look is active.
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
        update_mouse_look(ui, console_window, state, window);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            ui.focus_time = event.window.timestamp;
        }
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
    update_mouse_look(ui, console_window, state, window);
    return true;
}

/// Frames longer than this, e.g. after the window was dragged, count as this long to avoid jumps.
constexpr float kMaxFrameTime = 0.1f;

/// Reads the keys that are held down, and the click, the mouse look and the spells cast in this frame over the game.
///
/// These are the key bindings, with those of handle_events(). Keep GAME_MANUAL.md up to date when changing
/// them.
///
/// \param ui     Where the left button was pressed over the game, see Ui::game_click, the motion of mouse look, see
///               Ui::mouse_motion, and the spells cast, see Ui::casts.
/// \param view   How the game is shown. While a camera put in place shows the picture, clicks are ignored, since they
///               activate what the player's eyes see at that point of the picture, or at its center.
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
        .mouse_rotation = manifold::mouse_look_rotation(ui.mouse_motion, ui.mouse_sensitivity),
        .forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S),
        .run = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT],
        .strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A),
        .rise = axis(SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN),
        .activate = keys[SDL_SCANCODE_E],
        .click = click,
        .center_click = ui.center_click && !view.camera && !manifold::is_focus_click(*ui.center_click, ui.focus_time),
        .casts = ui.casts,
    };
}

/// Draws the game into the bound framebuffer, as `view` shows it, with the crosshair if the view has it, the red of a
/// landing that hurt the party, see manifold::damage_flash_opacity(), the portraits of the party, the spells in effect
/// and the bar of mana.
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
    if (const std::optional<manifold::FallImpact>& impact = game.last_fall_impact()) {
        manifold::OverlayMesh flash;
        const float opacity =
            manifold::damage_flash_opacity(static_cast<float>(game.time() - impact->time), impact->damage);
        manifold::add_damage_flash(flash, {.width = viewport.width, .height = viewport.height}, opacity);
        overlay_renderer.draw(flash, viewport);
    }
    portraits.draw(game.party(), overlay_renderer, viewport);
    manifold::OverlayMesh magic;
    manifold::add_spell_hud(magic, {.width = viewport.width, .height = viewport.height}, game.magic(),
                            game.magic_config());
    overlay_renderer.draw(magic, viewport);
}

/// Draws the game into the bound framebuffer, as draw_game() does, with the open screens of the interface over it, or
/// only the screens if one of them hides the game. Each screen draws its shapes and then the icons of its items.
void draw_scene(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
                const PartyPortraits& portraits, const ItemIcons& icons, const manifold::Game& game,
                const manifold::ViewSettings& view, const manifold::ScreenStack& screens,
                const manifold::PixelSize& framebuffer) {
    if (screens.hides_game()) {
        manifold::clear_to_black();
    } else {
        draw_game(renderer, overlay_renderer, portraits, game, view, framebuffer);
    }
    if (!screens.empty()) {
        const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
        for (const manifold::ScreenLayer& layer :
             screens.build_layers({.width = viewport.width, .height = viewport.height})) {
            overlay_renderer.draw(layer.mesh, viewport);
            icons.draw(layer.icons, viewport);
            overlay_renderer.draw(layer.over_icons, viewport);
        }
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
                      const PartyPortraits& portraits, const ItemIcons& icons, const manifold::Game& game,
                      const manifold::ScreenStack& screens, std::span<const manifold::ScreenshotRequest> requests,
                      const manifold::PixelSize& window_size) {
    for (const manifold::ScreenshotRequest& request : requests) {
        const manifold::PixelSize size = request.size.value_or(window_size);
        try {
            const manifold::OffscreenFramebuffer framebuffer(size);
            draw_scene(renderer, overlay_renderer, portraits, icons, game, request.view, screens, size);
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
/// \param saves The saves there are to load, which the menus offer only then.
void open_menu(manifold::MenuRequest request, manifold::ScreenStack& screens, manifold::SavesToLoad saves) {
    screens.clear();
    switch (request) {
        case manifold::MenuRequest::kStart:
            screens.push(manifold::make_start_menu(saves));
            break;
        case manifold::MenuRequest::kGame:
            screens.push(manifold::make_game_menu(saves));
            break;
        case manifold::MenuRequest::kClose:
            break;
    }
}

/// Closes the screens that show something of the level, such as what a chest holds, if another level started since they
/// were opened, see manifold::Screen::shows_level().
void close_old_level_screens(Ui& ui, const manifold::Game& game) {
    if (game.level_starts() != ui.level_starts) {
        ui.screens.close_level_screens();
        ui.level_starts = game.level_starts();
    }
}

/// Opens the inventory that `request` asks for over the open screens, instead of another one that is open. A chest
/// that the level doesn't have is logged as a warning.
void open_inventory(const manifold::InventoryRequest& request, Ui& ui, manifold::Game& game) {
    close_old_level_screens(ui, game);
    ui.screens.close_level_screens();
    if (!request.chest) {
        ui.screens.push(std::make_unique<manifold::InventoryScreen>(game.items(), game.inventory()));
        return;
    }
    if (*request.chest >= game.world().chests().size()) {
        manifold::log_warning("The level has no chest {}", *request.chest);
        return;
    }
    const manifold::Chest& chest = game.world().chests()[*request.chest];
    ui.screens.push(std::make_unique<manifold::InventoryScreen>(game.items(), game.inventory(),
                                                                game.chest_inventory(*request.chest),
                                                                std::string(manifold::chest_title(chest.look))));
}

/// Plays the sounds of the doors and chests of this frame, see play_object_sounds(), and shows what the chest that was
/// activated holds, the last one if there were several.
void follow_objects(manifold::Game& game, Ui& ui, const ObjectSounds& sounds, manifold::AudioOutput& audio) {
    const std::vector<std::size_t> chests = game.take_chest_activations();
    play_object_sounds(game, chests, sounds, audio);
    if (!chests.empty()) {
        open_inventory({.chest = chests.back()}, ui, game);
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
            if (level.load(game, state.view, manifold::SaveKind::kProgress, false)) {
                screens.clear();
            }
            break;
        case manifold::UiCommand::kLoadLevelStart:
            if (level.load(game, state.view, manifold::SaveKind::kLevelStart, false)) {
                screens.clear();
            }
            break;
        case manifold::UiCommand::kExit:
            state.quit = true;
            break;
    }
}

/// Does what was asked for since the last frame, at the start of a frame: what a screen of the interface asked for,
/// the menu that Escape or the `menu` command asked for, loading, and the inventory that I or the `open` command asked
/// for. Saves the new game that the start menu offered
/// once no screen is open, and then opens the Game over menu if the whole party is dead and no screen is open.
///
/// \param layout The layout of the first level of a new game, if given on the command line.
/// \throws std::runtime_error if the `load` command asked to load and to stop on error, and loading fails.
void apply_requests(Ui& ui, manifold::ConsoleState& state, LevelSession& level, manifold::Game& game,
                    std::optional<manifold::LayoutKind> layout) {
    if (const std::optional<manifold::UiCommand> command = std::exchange(ui.command, std::nullopt)) {
        apply_ui_command(*command, ui.screens, level, game, state, layout);
    }
    if (const std::optional<manifold::MenuRequest> menu = std::exchange(state.menu, std::nullopt)) {
        open_menu(*menu, ui.screens, level.saves());
    }
    // Loading comes before the commands, so that those after `load` run in the loaded level. As from a menu, loading
    // closes the menus.
    if (const std::optional<manifold::SaveKind> load = std::exchange(state.load, std::nullopt);
        load && level.load(game, state.view, *load, state.load_stops_on_error)) {
        ui.screens.clear();
    }
    // The new game is saved once the start menu is closed without continuing the save, also by the `menu` command.
    if (ui.screens.empty()) {
        level.save_deferred(game);
    }
    // Once the whole party is dead, `save.json` is deleted, so that the game can't go back to just before the death,
    // and the Game over menu pauses the game until the player restarts the level, starts a new game or exits. Other
    // screens, such as the start menu, come first, and closing it with the `menu` command opens it again.
    if (game.defeated() && ui.screens.empty()) {
        level.discard_progress();
        ui.screens.push(manifold::make_game_over_menu(level.saves()));
    }
    // The inventory opens over the menus that are open, and in the loaded level.
    if (const std::optional<manifold::InventoryRequest> inventory = std::exchange(state.inventory, std::nullopt)) {
        open_inventory(*inventory, ui, game);
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

    const GameAssets game_assets = load_assets();
    const manifold::AssetMapping& assets = game_assets.mapping;
    manifold::GameConfig config = game_assets.data.game;
    config.level.assets = assets.counts();
    manifold::Game game(config, options.seed.value_or(random_seed()), options.layout);

    const std::optional<std::filesystem::path> data_dir = user_data_dir();
    // Scripts of console commands mostly test the game and take screenshots, so they run in a window, which doesn't
    // take over the screen.
    const bool scripted = !options.commands.empty();
    // Nor do they take the mouse.
    const manifold::Settings settings = read_settings(data_dir);
    const bool fullscreen = settings.fullscreen && !scripted;
    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height, fullscreen);
    manifold::ConsoleState state;
    state.view.lights = game_assets.data.lights;
    state.view.data_lights = game_assets.data.lights;
    manifold::AudioOutput audio;
    const ObjectSounds object_sounds(assets);
    const manifold::SpellSounds spell_sounds(assets);
    FootstepSounds footsteps(assets);
    const manifold::OverlayRenderer overlay_renderer;
    PartyPortraits portraits(assets);
    const ItemIcons icons(assets, game.items());
    // Each level is saved when it starts, to both saves, which replaces the saves of the level before, and the game is
    // saved to save.json every autosave interval and on exit. Scripts of console commands mostly test the game, so
    // they leave the player's saves alone. If there is a save, the player is asked first whether to continue it, and
    // the new game is saved only once the start menu is closed without continuing.
    const bool saving = !scripted;
    std::optional<SavePaths> save_paths = save_file_paths(data_dir);
    const manifold::SavesToLoad saves = saves_to_continue(save_paths, saving);
    const bool ask = saves.progress || saves.level_start;
    LevelSession level(game, state.view, assets, std::move(save_paths), saving, autosave_interval(settings), ask);
    Ui ui;
    ui.mouse_look = settings.mouse_look && !scripted;
    ui.mouse_sensitivity = settings.mouse_sensitivity;
    ui.level_starts = game.level_starts();
    if (ask) {
        ui.screens.push(manifold::make_start_menu(saves));
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
        ui.center_click.reset();
        ui.mouse_motion = glm::vec2(0.0f);
        ui.casts.clear();
        // The player reached the exit, or a console command started the next level as if they had.
        level.follow(game, state.view, audio);
        level.autosave(game);
        close_old_level_screens(ui, game);
        follow_objects(game, ui, object_sounds, audio);
        play_spell_sounds(game, spell_sounds, audio);
        footsteps.play(game, audio);
        // A new game or a loaded one has another party.
        portraits.follow(game.party());

        const manifold::PixelSize window_size = window.framebuffer_size();
        const manifold::Viewport viewport = manifold::fit_viewport(window_size, manifold::kAspectRatio);
        ui.picture = {.width = viewport.width, .height = viewport.height};
        draw_scene(level.renderer(), overlay_renderer, portraits, icons, game, state.view, ui.screens, window_size);
        // The stats of this draw, before screenshots draw again.
        const manifold::RenderStats render_stats = level.renderer().stats();
        // A screen that hides the game hides the overlay too, whose numbers would be of the last drawing of the game.
        if (state.debug_overlay && !ui.screens.hides_game()) {
            overlay.draw(game, state.view, render_stats, frame_times);
        }
        if (console_window.is_open()) {
            overlay_renderer.draw(console_window.build_mesh(ui.picture), viewport);
        }
        take_screenshots(level.renderer(), overlay_renderer, portraits, icons, game, ui.screens, state.screenshots,
                         window_size);
        state.screenshots.clear();
        if (state.quit) {
            break;
        }
        window.swap_buffers();
        // The console window has the keys while it is open, and the screens have the keys and the mouse, so the
        // overlay gets no events then.
        overlay.set_input(state.debug_overlay && !console_window.is_open() && ui.screens.empty());
        update_mouse_look(ui, console_window, state, window);
    } while (handle_events(window, console_window, commands, overlay, state, ui, console_key_held, game));
    level.save_on_exit(game);
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
