#include "util/asset_mapping.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "level/texture_kind.hpp"
#include "util/asset_index.hpp"
#include "util/log.hpp"
#include "util/random.hpp"
#include "util/text_file.hpp"

namespace manifold {

std::string_view asset_key_name(AssetKey key) {
    switch (key) {
        case AssetKey::kDoorOpenSounds:
            return "DOOR_OPEN_SOUNDS";
        case AssetKey::kDoorCloseSounds:
            return "DOOR_CLOSE_SOUNDS";
        case AssetKey::kChestOpenSounds:
            return "CHEST_OPEN_SOUNDS";
        case AssetKey::kExitSounds:
            return "EXIT_SOUNDS";
        case AssetKey::kGrassFootstepSounds:
            return "FOOTSTEP_SOUNDS_GRASS";
        case AssetKey::kWoodFootstepSounds:
            return "FOOTSTEP_SOUNDS_WOOD";
        case AssetKey::kMetalFootstepSounds:
            return "FOOTSTEP_SOUNDS_METAL";
        case AssetKey::kStoneFootstepSounds:
            return "FOOTSTEP_SOUNDS_STONE";
        case AssetKey::kGrassLevelTextures:
            return "LEVEL_TEXTURES_GRASS";
        case AssetKey::kWoodLevelTextures:
            return "LEVEL_TEXTURES_WOOD";
        case AssetKey::kMetalLevelTextures:
            return "LEVEL_TEXTURES_METAL";
        case AssetKey::kStoneLevelTextures:
            return "LEVEL_TEXTURES_STONE";
        case AssetKey::kMalePortraits:
            return "PORTRAITS_MALE";
        case AssetKey::kFemalePortraits:
            return "PORTRAITS_FEMALE";
        case AssetKey::kAnyPortraits:
            return "PORTRAITS_ANY";
    }
    return "UNKNOWN";
}

std::optional<AssetKey> parse_asset_key_name(std::string_view name) {
    for (const AssetKey key : kAssetKeys) {
        if (name == asset_key_name(key)) {
            return key;
        }
    }
    return std::nullopt;
}

AssetType asset_key_type(AssetKey key) {
    switch (key) {
        case AssetKey::kDoorOpenSounds:
        case AssetKey::kDoorCloseSounds:
        case AssetKey::kChestOpenSounds:
        case AssetKey::kExitSounds:
        case AssetKey::kGrassFootstepSounds:
        case AssetKey::kWoodFootstepSounds:
        case AssetKey::kMetalFootstepSounds:
        case AssetKey::kStoneFootstepSounds:
            return AssetType::kSound;
        case AssetKey::kGrassLevelTextures:
        case AssetKey::kWoodLevelTextures:
        case AssetKey::kMetalLevelTextures:
        case AssetKey::kStoneLevelTextures:
            return AssetType::kLevelTexture;
        case AssetKey::kMalePortraits:
        case AssetKey::kFemalePortraits:
        case AssetKey::kAnyPortraits:
            return AssetType::kPortrait;
    }
    return AssetType::kSound;
}

AssetKey footstep_sounds_key(TextureKind kind) {
    switch (kind) {
        case TextureKind::kGrass:
            return AssetKey::kGrassFootstepSounds;
        case TextureKind::kWood:
            return AssetKey::kWoodFootstepSounds;
        case TextureKind::kMetal:
            return AssetKey::kMetalFootstepSounds;
        case TextureKind::kStone:
            return AssetKey::kStoneFootstepSounds;
    }
    return AssetKey::kStoneFootstepSounds;
}

AssetKey level_textures_key(TextureKind kind) {
    switch (kind) {
        case TextureKind::kGrass:
            return AssetKey::kGrassLevelTextures;
        case TextureKind::kWood:
            return AssetKey::kWoodLevelTextures;
        case TextureKind::kMetal:
            return AssetKey::kMetalLevelTextures;
        case TextureKind::kStone:
            return AssetKey::kStoneLevelTextures;
    }
    return AssetKey::kStoneLevelTextures;
}

AssetIndex choose_asset(Rng& rng, const AssetCounts& counts, AssetKey key) {
    const std::uint32_t count = counts.of(key);
    if (count == 0) {
        return 0;
    }
    return static_cast<AssetIndex>(random_index(rng, count));
}

AssetMapping AssetMapping::parse(std::string_view json, const std::filesystem::path& assets_dir,
                                 std::string_view source) {
    AssetMapping mapping;
    nlohmann::json object;
    try {
        object = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error& error) {
        log_warning("Asset mapping {} is not valid JSON, so no assets are used: {}", source, error.what());
        return mapping;
    }
    if (!object.is_object()) {
        log_warning("Asset mapping {} is not a JSON object, so no assets are used", source);
        return mapping;
    }
    for (const auto& [name, value] : object.items()) {
        const std::optional<AssetKey> key = parse_asset_key_name(name);
        if (!key) {
            log_warning("Asset mapping {} has an unknown key {}, which is ignored", source, name);
            continue;
        }
        std::vector<std::filesystem::path>& paths = mapping.paths_.at(static_cast<std::size_t>(*key));
        if (value.is_string()) {
            paths.push_back(assets_dir / value.get<std::string>());
        } else if (value.is_array()) {
            for (const nlohmann::json& entry : value) {
                if (entry.is_string()) {
                    paths.push_back(assets_dir / entry.get<std::string>());
                } else {
                    log_warning("Asset mapping {}: {} has an entry {} that is not a path, so asset {} has no file",
                                source, name, entry.dump(), paths.size());
                    paths.emplace_back();
                }
            }
        } else {
            log_warning("Asset mapping {}: {} is {}, not a path or an array of paths, so it is ignored", source, name,
                        value.dump());
        }
    }
    for (const AssetKey key : kAssetKeys) {
        if (mapping.paths(key).empty()) {
            log_warning("Asset mapping {} has no files for {}", source, asset_key_name(key));
        }
    }
    return mapping;
}

AssetMapping AssetMapping::load(const std::filesystem::path& assets_dir) {
    const std::filesystem::path file = assets_dir / kAssetMappingFileName;
    std::string json;
    try {
        json = read_text_file(file);
    } catch (const std::runtime_error& error) {
        log_warning("Can't read the asset mapping, so no assets are used: {}", error.what());
        return {};
    }
    AssetMapping mapping = parse(json, assets_dir, file.string());
    std::size_t count = 0;
    for (const std::vector<std::filesystem::path>& paths : mapping.paths_) {
        count += paths.size();
    }
    log_info("Asset mapping {}: {} files for {} keys", file.string(), count, kAssetKeys.size());
    return mapping;
}

std::optional<std::filesystem::path> AssetMapping::path(AssetKey key, AssetIndex index) const {
    const std::span<const std::filesystem::path> files = paths(key);
    if (index >= files.size()) {
        log_warning("{} has no asset {}: it has {}", asset_key_name(key), index, files.size());
        return std::nullopt;
    }
    return files[index];
}

AssetCounts AssetMapping::counts() const {
    AssetCounts counts;
    for (const AssetKey key : kAssetKeys) {
        counts.set(key, static_cast<std::uint32_t>(paths(key).size()));
    }
    return counts;
}

}  // namespace manifold
