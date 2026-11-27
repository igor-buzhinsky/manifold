#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "level/texture_kind.hpp"
#include "util/asset_index.hpp"
#include "util/random.hpp"

namespace manifold {

/// What the files of a key of the asset mapping must be.
enum class AssetType : std::uint8_t {
    kSound,         ///< An OGG Vorbis file, see load_sound().
    kLevelTexture,  ///< A PNG file of at most kMaxTextureSize pixels in each dimension, see load_level_texture().
    kPortrait,      ///< A PNG file, see load_image().
};

/// The keys of the asset mapping: what the game uses assets for. Each key has a list of files, see AssetMapping.
enum class AssetKey : std::uint8_t {
    kDoorOpenSounds,       ///< A door starts to open.
    kDoorCloseSounds,      ///< A door starts to close.
    kChestOpenSounds,      ///< A chest is activated.
    kExitSounds,           ///< The player enters the exit portal of a level.
    kGrassFootstepSounds,  ///< Footsteps in levels of TextureKind::kGrass.
    kWoodFootstepSounds,   ///< Footsteps in levels of TextureKind::kWood.
    kMetalFootstepSounds,  ///< Footsteps in levels of TextureKind::kMetal.
    kStoneFootstepSounds,  ///< Footsteps in levels of TextureKind::kStone.
    kGrassLevelTextures,   ///< Textures of levels of TextureKind::kGrass.
    kWoodLevelTextures,    ///< Textures of levels of TextureKind::kWood.
    kMetalLevelTextures,   ///< Textures of levels of TextureKind::kMetal.
    kStoneLevelTextures,   ///< Textures of levels of TextureKind::kStone.
    kMalePortraits,        ///< Portraits of male characters.
    kFemalePortraits,      ///< Portraits of female characters.
    kAnyPortraits,         ///< Portraits of characters of either gender, e.g. a skeleton.
};

/// All keys of the asset mapping, in the order of AssetKey.
inline constexpr std::array<AssetKey, 15> kAssetKeys{
    AssetKey::kDoorOpenSounds,      AssetKey::kDoorCloseSounds,     AssetKey::kChestOpenSounds,
    AssetKey::kExitSounds,          AssetKey::kGrassFootstepSounds, AssetKey::kWoodFootstepSounds,
    AssetKey::kMetalFootstepSounds, AssetKey::kStoneFootstepSounds, AssetKey::kGrassLevelTextures,
    AssetKey::kWoodLevelTextures,   AssetKey::kMetalLevelTextures,  AssetKey::kStoneLevelTextures,
    AssetKey::kMalePortraits,       AssetKey::kFemalePortraits,     AssetKey::kAnyPortraits,
};

/// The name of a key in the asset mapping file, e.g. `DOOR_OPEN_SOUNDS`, `FOOTSTEP_SOUNDS_GRASS`,
/// `LEVEL_TEXTURES_GRASS` or `PORTRAITS_MALE`.
std::string_view asset_key_name(AssetKey key);

/// The key named by asset_key_name(), or nothing if `name` names none.
std::optional<AssetKey> parse_asset_key_name(std::string_view name);

/// What the files of a key must be.
AssetType asset_key_type(AssetKey key);

/// The key of the footstep sounds of levels of a texture kind, e.g. AssetKey::kGrassFootstepSounds.
AssetKey footstep_sounds_key(TextureKind kind);

/// The key of the textures of levels of a texture kind, e.g. AssetKey::kGrassLevelTextures.
AssetKey level_textures_key(TextureKind kind);

/// How many files each key of the asset mapping lists, which is all that the random choices of assets depend on, see
/// choose_asset(). None for each key unless set.
class AssetCounts {
public:
    /// How many files `key` lists.
    std::uint32_t of(AssetKey key) const { return counts_.at(static_cast<std::size_t>(key)); }
    void set(AssetKey key, std::uint32_t count) { counts_.at(static_cast<std::size_t>(key)) = count; }

    bool operator==(const AssetCounts&) const = default;

private:
    std::array<std::uint32_t, kAssetKeys.size()> counts_{};
};

/// Chooses one of the files of `key` at random, each with the same chance.
///
/// \return An index in [0, counts.of(key)), or 0 if the key has no files, which then stands for no asset, see
///         AssetMapping::path(). Without files, no random number is used.
AssetIndex choose_asset(Rng& rng, const AssetCounts& counts, AssetKey key);

/// The key of the asset mapping whose value is an object from item keys to their icons, see
/// AssetMapping::item_icon(). Unlike the keys of AssetKey, it names one file per item rather than a list.
inline constexpr std::string_view kItemIconsKey = "ITEM_ICONS";

/// The key of the asset mapping whose value is an object from spell keys, see spell_key(), to their casting sounds,
/// see AssetMapping::spell_sound(). Like kItemIconsKey, it names one file per spell rather than a list.
inline constexpr std::string_view kSpellSoundsKey = "SPELL_SOUNDS";

/// Name of the file of the asset mapping, in the directory of the assets.
inline constexpr std::string_view kAssetMappingFileName = "asset_mapping.json";

/// Which files the game uses for what: a list of files for each AssetKey, read from a JSON file, so that assets can be
/// remapped without building the game again, and one file can serve several keys.
///
/// The file is an object whose fields are keys, named by asset_key_name(), and whose values are a path or an array of
/// paths, e.g. `{"DOOR_OPEN_SOUNDS": ["sounds/door_open/a.ogg", "sounds/door_open/b.ogg"],
/// "EXIT_SOUNDS": "sounds/portal/c.ogg"}`. Relative paths are relative to the directory of the assets.
///
/// The order of the files counts: an AssetIndex is a position in the list of its key. A file that is missing, can't be
/// read or isn't of the key's AssetType keeps its position, but gives no sound or a default texture, after a warning.
///
/// Besides these, the field kItemIconsKey (`ITEM_ICONS`) is an object from the keys of items, see ItemCatalog, to the
/// paths of their icons: PNG files with a transparent background, e.g.
/// `{"ITEM_ICONS": {"small_ruby": "inventory/gems/ruby-small.png"}}`, and the field kSpellSoundsKey (`SPELL_SOUNDS`)
/// is an object from the keys of spells to the paths of the OGG Vorbis files played when they are cast, e.g.
/// `{"SPELL_SOUNDS": {"levitate": "sounds/spells/shimmer.ogg"}}`.
class AssetMapping {
public:
    /// A mapping in which no key has files.
    AssetMapping() = default;

    /// Reads a mapping from the text of its JSON file.
    ///
    /// Problems don't stop it, but are logged as warnings, with `source` (e.g. the file's path): text that isn't a
    /// JSON object gives a mapping without files; an unknown key, or a value that isn't a string or an array, is left
    /// out; an entry of an array that isn't a string gives an empty path, which can't be loaded, so that the entries
    /// after it keep their positions; and a key that is missing or has no files gets none. In `ITEM_ICONS`, a value
    /// that isn't an object gives no icons, and an entry that isn't a string is left out; so it is in `SPELL_SOUNDS`.
    ///
    /// \param assets_dir What relative paths are relative to.
    static AssetMapping parse(std::string_view json, const std::filesystem::path& assets_dir, std::string_view source);

    /// Reads the mapping of the game: kAssetMappingFileName in `assets_dir`, see parse(). A file that can't be read
    /// gives a mapping without files, after a warning. How many files the keys have in all, and how many item icons and
    /// spell sounds there are, is logged as info.
    static AssetMapping load(const std::filesystem::path& assets_dir);

    /// The files of `key`, in their order in the mapping, with relative paths made absolute.
    std::span<const std::filesystem::path> paths(AssetKey key) const {
        return paths_.at(static_cast<std::size_t>(key));
    }

    /// File `index` of `key`.
    ///
    /// \return The path, or an empty optional (after logging a warning) if the key has no such file, e.g. if the
    ///         mapping changed since the index was chosen.
    std::optional<std::filesystem::path> path(AssetKey key, AssetIndex index) const;

    /// How many files each key has.
    AssetCounts counts() const;

    /// The icon of the item with the key `item_key`, e.g. `small_ruby`, from `ITEM_ICONS`, with a relative path made
    /// absolute, or nothing if it has none.
    std::optional<std::filesystem::path> item_icon(std::string_view item_key) const;

    /// All icons of `ITEM_ICONS`, by the keys of their items.
    const std::map<std::string, std::filesystem::path, std::less<>>& item_icons() const { return item_icons_; }

    /// The casting sound of the spell with the key `spell_key`, e.g. `levitate`, from `SPELL_SOUNDS`, with a relative
    /// path made absolute, or nothing if it has none.
    std::optional<std::filesystem::path> spell_sound(std::string_view spell_key) const;

    /// All sounds of `SPELL_SOUNDS`, by the keys of their spells.
    const std::map<std::string, std::filesystem::path, std::less<>>& spell_sounds() const { return spell_sounds_; }

private:
    std::array<std::vector<std::filesystem::path>, kAssetKeys.size()> paths_;
    std::map<std::string, std::filesystem::path, std::less<>> item_icons_;    ///< See item_icons().
    std::map<std::string, std::filesystem::path, std::less<>> spell_sounds_;  ///< See spell_sounds().
};

}  // namespace manifold
