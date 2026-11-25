#include "util/asset_mapping.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "audio/sound.hpp"
#include "level/texture_kind.hpp"
#include "render/texture_image.hpp"
#include "util/asset_files.hpp"
#include "util/asset_index.hpp"
#include "util/random.hpp"

namespace manifold {
namespace {

TEST(AssetMappingTest, KeyNamesAreUniqueAndRoundTrip) {
    std::set<std::string_view> names;
    for (const AssetKey key : kAssetKeys) {
        EXPECT_EQ(parse_asset_key_name(asset_key_name(key)), key);
        EXPECT_TRUE(names.insert(asset_key_name(key)).second) << asset_key_name(key);
    }
    EXPECT_FALSE(parse_asset_key_name("door_open_sounds").has_value());
    EXPECT_FALSE(parse_asset_key_name("UNKNOWN").has_value());
}

TEST(AssetMappingTest, EachTextureKindHasItsFootstepsAndTextures) {
    std::set<AssetKey> keys;
    for (const TextureKind kind : kTextureKinds) {
        const AssetKey footsteps = footstep_sounds_key(kind);
        const AssetKey textures = level_textures_key(kind);
        EXPECT_EQ(asset_key_type(footsteps), AssetType::kSound);
        EXPECT_EQ(asset_key_type(textures), AssetType::kLevelTexture);
        EXPECT_TRUE(keys.insert(footsteps).second);
        EXPECT_TRUE(keys.insert(textures).second);
    }
    EXPECT_EQ(asset_key_name(footstep_sounds_key(TextureKind::kGrass)), "FOOTSTEP_SOUNDS_GRASS");
    EXPECT_EQ(asset_key_name(level_textures_key(TextureKind::kStone)), "LEVEL_TEXTURES_STONE");
    EXPECT_EQ(asset_key_type(AssetKey::kChestOpenSounds), AssetType::kSound);
    EXPECT_EQ(asset_key_type(AssetKey::kAnyPortraits), AssetType::kPortrait);
}

TEST(AssetMappingTest, ChoiceIsAnIndexOfTheFilesAndReachesEach) {
    AssetCounts counts;
    counts.set(AssetKey::kDoorOpenSounds, 3);
    std::set<AssetIndex> chosen;
    Rng rng(1);
    for (int i = 0; i < 100; ++i) {
        const AssetIndex index = choose_asset(rng, counts, AssetKey::kDoorOpenSounds);
        EXPECT_LT(index, 3U);
        chosen.insert(index);
    }
    EXPECT_EQ(chosen, (std::set<AssetIndex>{0, 1, 2}));
}

TEST(AssetMappingTest, ChoiceWithoutFilesIsZeroAndUsesNoRandomNumbers) {
    Rng rng(5);
    Rng unused(5);
    EXPECT_EQ(choose_asset(rng, AssetCounts{}, AssetKey::kExitSounds), 0U);
    EXPECT_EQ(rng(), unused());
}

/// Parses `json` as a mapping of the directory `/assets`, and returns what was logged in `log`.
AssetMapping parse(std::string_view json, std::string& log) {
    testing::internal::CaptureStderr();
    AssetMapping mapping = AssetMapping::parse(json, "/assets", "test.json");
    log = testing::internal::GetCapturedStderr();
    return mapping;
}

TEST(AssetMappingTest, ParsesPathsAndListsOfPathsRelativeToTheAssets) {
    constexpr const char* kJson = R"({
        "DOOR_OPEN_SOUNDS": ["sounds/a.ogg", "sounds/b.ogg"],
        "CHEST_OPEN_SOUNDS": ["sounds/b.ogg"],
        "EXIT_SOUNDS": "/elsewhere/c.ogg",
        "PORTRAITS_ANY": []
    })";
    std::string log;
    const AssetMapping mapping = parse(kJson, log);
    const std::span<const std::filesystem::path> door_open = mapping.paths(AssetKey::kDoorOpenSounds);
    EXPECT_EQ(std::vector(door_open.begin(), door_open.end()),
              (std::vector<std::filesystem::path>{"/assets/sounds/a.ogg", "/assets/sounds/b.ogg"}));
    EXPECT_EQ(mapping.path(AssetKey::kChestOpenSounds, 0), std::filesystem::path("/assets/sounds/b.ogg"));
    EXPECT_EQ(mapping.path(AssetKey::kExitSounds, 0), std::filesystem::path("/elsewhere/c.ogg"));
    EXPECT_TRUE(mapping.paths(AssetKey::kAnyPortraits).empty());
    EXPECT_EQ(mapping.counts().of(AssetKey::kDoorOpenSounds), 2U);
    EXPECT_EQ(mapping.counts().of(AssetKey::kExitSounds), 1U);
    EXPECT_EQ(mapping.counts().of(AssetKey::kMalePortraits), 0U);
    // Keys that are missing or empty are named.
    EXPECT_NE(log.find("[warning] Asset mapping test.json has no files for PORTRAITS_ANY"), std::string::npos) << log;
    EXPECT_NE(log.find("[warning] Asset mapping test.json has no files for PORTRAITS_MALE"), std::string::npos) << log;
    EXPECT_EQ(log.find("for DOOR_OPEN_SOUNDS"), std::string::npos) << log;
}

TEST(AssetMappingTest, WrongEntriesKeepThePositionsOfTheOthers) {
    constexpr const char* kJson = R"({
        "DOOR_OPEN_SOUNDS": [1, "b.ogg", null],
        "DOOR_CLOSE_SOUNDS": 5,
        "DOOR_SOUNDS": ["c.ogg"]
    })";
    std::string log;
    const AssetMapping mapping = parse(kJson, log);
    EXPECT_EQ(mapping.counts().of(AssetKey::kDoorOpenSounds), 3U);
    EXPECT_EQ(mapping.path(AssetKey::kDoorOpenSounds, 0), std::filesystem::path());
    EXPECT_EQ(mapping.path(AssetKey::kDoorOpenSounds, 1), std::filesystem::path("/assets/b.ogg"));
    EXPECT_EQ(mapping.counts().of(AssetKey::kDoorCloseSounds), 0U);
    EXPECT_NE(log.find("DOOR_OPEN_SOUNDS has an entry 1 that is not a path, so asset 0 has no file"), std::string::npos)
        << log;
    EXPECT_NE(log.find("DOOR_OPEN_SOUNDS has an entry null that is not a path, so asset 2 has no file"),
              std::string::npos)
        << log;
    EXPECT_NE(log.find("DOOR_CLOSE_SOUNDS is 5, not a path or an array of paths"), std::string::npos) << log;
    EXPECT_NE(log.find("unknown key DOOR_SOUNDS"), std::string::npos) << log;
}

TEST(AssetMappingTest, TextThatIsNotAnObjectGivesNoFiles) {
    for (const std::string_view json : {"", "not json", R"(["a.ogg"])"}) {
        std::string log;
        const AssetMapping mapping = parse(json, log);
        EXPECT_EQ(mapping.counts(), AssetCounts{}) << json;
        EXPECT_NE(log.find("[warning] Asset mapping test.json is not"), std::string::npos) << log;
    }
}

TEST(AssetMappingTest, IndexBeyondTheFilesGivesNothingWithAWarning) {
    std::string log;
    const AssetMapping mapping = parse(R"({"EXIT_SOUNDS": ["a.ogg"]})", log);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(mapping.path(AssetKey::kExitSounds, 1).has_value());
    EXPECT_FALSE(mapping.path(AssetKey::kMalePortraits, 0).has_value());
    log = testing::internal::GetCapturedStderr();
    EXPECT_NE(log.find("[warning] EXIT_SOUNDS has no asset 1: it has 1"), std::string::npos) << log;
    EXPECT_NE(log.find("[warning] PORTRAITS_MALE has no asset 0: it has 0"), std::string::npos) << log;
}

TEST(AssetMappingTest, MissingFileGivesNoFiles) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping =
        AssetMapping::load(std::filesystem::temp_directory_path() / "manifold_AssetMappingTest_missing");
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(mapping.counts(), AssetCounts{});
    EXPECT_NE(log.find("[warning] Can't read the asset mapping, so no assets are used"), std::string::npos) << log;
}

/// Whether `path` is a file of the type that `key` needs. Logs why not.
bool is_usable(AssetKey key, const std::filesystem::path& path) {
    switch (asset_key_type(key)) {
        case AssetType::kSound:
            return load_sound(path).has_value();
        case AssetType::kLevelTexture:
            return load_level_texture(path).has_value();
        case AssetType::kPortrait:
            return load_image(path).has_value();
    }
    return false;
}

TEST(GameAssetMappingTest, EveryKeyHasFilesAndAllCanBeUsed) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping = AssetMapping::load(MANIFOLD_ASSETS_DIR);
    std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
    for (const AssetKey key : kAssetKeys) {
        EXPECT_FALSE(mapping.paths(key).empty()) << asset_key_name(key);
        for (const std::filesystem::path& path : mapping.paths(key)) {
            testing::internal::CaptureStderr();
            EXPECT_TRUE(is_usable(key, path)) << asset_key_name(key) << ": " << path;
            log = testing::internal::GetCapturedStderr();
            EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
        }
    }
}

TEST(GameAssetMappingTest, EverySoundAndImageOfTheAssetsIsMapped) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping = AssetMapping::load(MANIFOLD_ASSETS_DIR);
    testing::internal::GetCapturedStderr();
    std::set<std::filesystem::path> mapped;
    for (const AssetKey key : kAssetKeys) {
        for (const std::filesystem::path& path : mapping.paths(key)) {
            mapped.insert(path.lexically_normal());
        }
    }
    for (const std::string_view extension : {".ogg", ".png"}) {
        for (const std::filesystem::path& path : find_asset_files(MANIFOLD_ASSETS_DIR, extension)) {
            EXPECT_TRUE(mapped.contains(path.lexically_normal())) << path << " is in no key of the asset mapping";
        }
    }
}

}  // namespace
}  // namespace manifold
