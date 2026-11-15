#include "render/lighting.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

TEST(LightingTest, LuminanceWeighsGreenMostAndBlueLeast) {
    EXPECT_FLOAT_EQ(luminance(glm::vec3(1.0f)), 1.0f);
    EXPECT_FLOAT_EQ(luminance({1.0f, 0.0f, 0.0f}), 0.2126f);
    EXPECT_FLOAT_EQ(luminance({0.0f, 1.0f, 0.0f}), 0.7152f);
    EXPECT_FLOAT_EQ(luminance({0.0f, 0.0f, 1.0f}), 0.0722f);
}

TEST(LightingTest, LevelAmbientIsNoneOrTextureHueForAboutHalfOfSeeds) {
    const glm::vec3 color{0.3f, 0.2f, 0.1f};
    int lit = 0;
    constexpr int kSeeds = 1000;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const glm::vec3 ambient = level_ambient(seed, color);
        EXPECT_EQ(level_ambient(seed, color), ambient) << "seed " << seed;
        if (ambient == glm::vec3(0.0f)) {
            continue;
        }
        ++lit;
        EXPECT_NEAR(luminance(ambient), kLevelAmbientLuminance, 1e-6f) << "seed " << seed;
        EXPECT_NEAR(ambient.r / ambient.b, 3.0f, 1e-5f) << "seed " << seed;
        EXPECT_NEAR(ambient.g / ambient.b, 2.0f, 1e-5f) << "seed " << seed;
    }
    EXPECT_GT(lit, 400);
    EXPECT_LT(lit, 600);
}

TEST(LightingTest, LevelAmbientIsAsBrightForDarkAndLightTextures) {
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const glm::vec3 dark = level_ambient(seed, glm::vec3(0.05f));
        const glm::vec3 light = level_ambient(seed, glm::vec3(0.8f));
        for (int i = 0; i < 3; ++i) {
            EXPECT_NEAR(dark[i], light[i], 1e-6f) << "seed " << seed;
        }
    }
}

TEST(LightingTest, LevelAmbientKeepsTheHueOfSaturatedTextures) {
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const glm::vec3 ambient = level_ambient(seed, {0.0f, 0.0f, 0.5f});
        if (ambient != glm::vec3(0.0f)) {
            EXPECT_EQ(ambient.r, 0.0f);
            EXPECT_EQ(ambient.g, 0.0f);
            EXPECT_NEAR(ambient.b, kLevelAmbientLuminance / 0.0722f, 1e-5f);  // Above 1.
        }
    }
}

TEST(LightingTest, LevelAmbientOfNearBlackTextureIsGrey) {
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const glm::vec3 ambient = level_ambient(seed, {0.005f, 0.0f, 0.0f});
        EXPECT_TRUE(ambient == glm::vec3(0.0f) || ambient == glm::vec3(kLevelAmbientLuminance)) << "seed " << seed;
    }
}

TEST(LightingTest, TorchPhasesAreSpreadOverCircle) {
    std::vector<float> phases;
    for (std::size_t i = 0; i < 10; ++i) {
        const float phase = torch_phase(i);
        EXPECT_GE(phase, 0.0f);
        EXPECT_LT(phase, 2.0f * std::numbers::pi_v<float>);
        for (const float other : phases) {
            EXPECT_GT(std::abs(phase - other), 0.1f) << "torch " << i;
        }
        phases.push_back(phase);
    }
}

TEST(LightingTest, FlickerStaysInRangeAndChanges) {
    float lowest = 1.0f;
    float highest = 0.0f;
    for (int step = 0; step < 1000; ++step) {
        const float flicker = torch_flicker(static_cast<float>(step) * 0.01f, 1.0f);
        EXPECT_GE(flicker, 0.75f);
        EXPECT_LE(flicker, 1.0f);
        lowest = std::min(lowest, flicker);
        highest = std::max(highest, flicker);
    }
    EXPECT_GT(highest - lowest, 0.15f);
}

TEST(LightingTest, TorchesWithDifferentPhasesFlickerDifferently) {
    EXPECT_NE(torch_flicker(2.0f, torch_phase(0)), torch_flicker(2.0f, torch_phase(1)));
}

TEST(LightingTest, TorchLightStaysAtItsFlame) {
    const Torch torch{.mount = {3.0f, -2.0f, 1.6f}, .yaw = 1.0f};
    for (int step = 0; step < 100; ++step) {
        const float time = static_cast<float>(step) * 0.1f;
        const PointLight light = torch_light(torch, time, 0.5f);
        EXPECT_EQ(light.position, torch.light_position());
        EXPECT_FALSE(light.shadow_id.has_value());
        EXPECT_EQ(light.color, torch_light_color(TorchFlame::kOrange));
        EXPECT_FLOAT_EQ(light.intensity, kTorchIntensity * torch_flicker(time, 0.5f));
        EXPECT_FLOAT_EQ(light.range, kTorchRange);
    }
}

TEST(LightingTest, TorchLightHasColorOfItsFlame) {
    for (const TorchFlame flame : {TorchFlame::kOrange, TorchFlame::kBlue, TorchFlame::kGreen}) {
        const Torch torch{.mount = {0.0f, 0.0f, 1.6f}, .yaw = 0.0f, .flame = flame};
        EXPECT_EQ(torch_light(torch, 1.0f, 0.0f).color, torch_light_color(flame));
    }
    // Orange is warm, blue and green are cold: their brightest channel is not red.
    const glm::vec3 orange = torch_light_color(TorchFlame::kOrange);
    const glm::vec3 blue = torch_light_color(TorchFlame::kBlue);
    const glm::vec3 green = torch_light_color(TorchFlame::kGreen);
    EXPECT_GT(orange.r, orange.b);
    EXPECT_GT(blue.b, std::max(blue.r, blue.g));
    EXPECT_GT(green.g, std::max(green.r, green.b));
}

TEST(LightingTest, TorchLightsKeepTorchOrder) {
    const std::vector<Torch> torches{{.mount = {0.0f, 0.0f, 1.0f}, .yaw = 0.0f},
                                     {.mount = {5.0f, 0.0f, 1.0f}, .yaw = 0.0f}};
    const std::vector<PointLight> lights = torch_lights(torches, 1.5f);
    ASSERT_EQ(lights.size(), 2U);
    for (std::size_t i = 0; i < lights.size(); ++i) {
        const PointLight expected = torch_light(torches[i], 1.5f, torch_phase(i));
        EXPECT_EQ(lights[i].position, expected.position);
        EXPECT_FLOAT_EQ(lights[i].intensity, expected.intensity);
        EXPECT_EQ(lights[i].shadow_id, i);
    }
}

TEST(LightingTest, SelectsLightsWhoseRangeComesClosest) {
    const std::vector<PointLight> lights{
        {.position = {10.0f, 0.0f, 0.0f}, .range = 2.0f},   // 8 m away from its range.
        {.position = {3.0f, 0.0f, 0.0f}, .range = 1.0f},    // 2 m.
        {.position = {20.0f, 0.0f, 0.0f}, .range = 19.0f},  // 1 m: far, but its range is large.
        {.position = {0.0f, 6.0f, 0.0f}, .range = 1.0f},    // 5 m.
    };
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 3);
    ASSERT_EQ(selected.size(), 3U);
    EXPECT_EQ(selected[0].position, lights[2].position);
    EXPECT_EQ(selected[1].position, lights[1].position);
    EXPECT_EQ(selected[2].position, lights[3].position);
}

TEST(LightingTest, SelectsAllLightsWhenThereAreFewer) {
    const std::vector<PointLight> lights{{.position = {1.0f, 0.0f, 0.0f}}, {.position = {2.0f, 0.0f, 0.0f}}};
    EXPECT_EQ(select_lights(lights, glm::vec3(0.0f), kMaxLights).size(), 2U);
    EXPECT_TRUE(select_lights({}, glm::vec3(0.0f), kMaxLights).empty());
}

TEST(LightingTest, LightsAtSameDistanceKeepTheirOrder) {
    const std::vector<PointLight> lights{{.position = {1.0f, 0.0f, 0.0f}, .shadow_id = 1},
                                         {.position = {0.0f, 1.0f, 0.0f}, .shadow_id = 2},
                                         {.position = {-1.0f, 0.0f, 0.0f}, .shadow_id = 3}};
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 2);
    ASSERT_EQ(selected.size(), 2U);
    EXPECT_EQ(selected[0].shadow_id, 1U);
    EXPECT_EQ(selected[1].shadow_id, 2U);
}

TEST(LightingTest, AllLightsKeepIntensityWhenNoneIsLeftOut) {
    const std::vector<PointLight> lights{{.position = {1.0f, 0.0f, 0.0f}, .intensity = 2.0f},
                                         {.position = {30.0f, 0.0f, 0.0f}, .intensity = 3.0f}};
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 2);
    ASSERT_EQ(selected.size(), 2U);
    EXPECT_FLOAT_EQ(selected[0].intensity, 2.0f);
    EXPECT_FLOAT_EQ(selected[1].intensity, 3.0f);
}

TEST(LightingTest, LightsNearTheCutFadeOut) {
    // Gaps 1, 4, 5 and 7 m. The cut is the gap of the light that is left out: 7 m.
    const std::vector<PointLight> lights{{.position = {2.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {5.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {6.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {8.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f}};
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 3, 4.0f);
    ASSERT_EQ(selected.size(), 3U);
    EXPECT_FLOAT_EQ(selected[0].intensity, 2.0f);          // 6 m from the cut: more than the fade distance.
    EXPECT_FLOAT_EQ(selected[1].intensity, 2.0f * 0.75f);  // 3 m from the cut.
    EXPECT_FLOAT_EQ(selected[2].intensity, 2.0f * 0.5f);   // 2 m from the cut.
}

/// The intensity that select_lights() gives to the light with `id`, or 0 if it is left out.
float selected_intensity(const std::vector<PointLight>& selected, std::uint32_t id) {
    for (const PointLight& light : selected) {
        if (light.shadow_id == id) {
            return light.intensity;
        }
    }
    return 0.0f;
}

TEST(LightingTest, IntensitiesChangeSmoothlyWhenLightsSwap) {
    // Lights on both sides of a corridor. Walking along it, lights behind the viewer are replaced by lights
    // ahead of it, one at a time.
    std::vector<PointLight> lights;
    for (std::uint32_t i = 0; i < 20; ++i) {
        lights.push_back({.position = {static_cast<float>(i) * 3.0f, (i % 2 == 0) ? 2.0f : -2.0f, 0.0f},
                          .intensity = 1.0f,
                          .range = 6.0f,
                          .shadow_id = i});
    }
    constexpr float kStep = 0.01f;
    std::vector<PointLight> previous = select_lights(lights, glm::vec3(0.0f), 5);
    constexpr int kSteps = 5700;  // 57 m, past the last light.
    int swaps = 0;
    for (int step = 1; step <= kSteps; ++step) {
        const float x = static_cast<float>(step) * kStep;
        const std::vector<PointLight> selected = select_lights(lights, glm::vec3(x, 0.0f, 0.0f), 5);
        for (std::uint32_t id = 0; id < lights.size(); ++id) {
            // The gap and the cut each change by at most kStep, so the intensity by at most
            // 2 * kStep / kLightFadeDistance.
            ASSERT_NEAR(selected_intensity(selected, id), selected_intensity(previous, id),
                        (2.0f * kStep / kLightFadeDistance) + 1e-5f)
                << "light " << id << " at x = " << x;
        }
        const auto chosen = [](const std::vector<PointLight>& set, const PointLight& light) {
            return std::ranges::any_of(set,
                                       [&](const PointLight& other) { return other.shadow_id == light.shadow_id; });
        };
        swaps += static_cast<int>(
            std::ranges::count_if(previous, [&](const PointLight& light) { return !chosen(selected, light); }));
        previous = selected;
    }
    // Most lights were chosen and then left out again on the way.
    EXPECT_GE(swaps, 10);
}

TEST(LightingTest, LightTiedWithCutHasNoIntensity) {
    // Gaps 0 and 4 m twice: the second light at 4 m is left out, so the first one is at the cut.
    const std::vector<PointLight> lights{{.position = {1.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {5.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {-5.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f}};
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 2);
    ASSERT_EQ(selected.size(), 2U);
    EXPECT_FLOAT_EQ(selected[0].intensity, 2.0f);
    EXPECT_FLOAT_EQ(selected[1].intensity, 0.0f);
}

TEST(LightingTest, ZeroFadeDistanceGivesNoNan) {
    const std::vector<PointLight> lights{{.position = {5.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f},
                                         {.position = {-5.0f, 0.0f, 0.0f}, .intensity = 2.0f, .range = 1.0f}};
    const std::vector<PointLight> selected = select_lights(lights, glm::vec3(0.0f), 1, 0.0f);
    ASSERT_EQ(selected.size(), 1U);
    EXPECT_FLOAT_EQ(selected[0].intensity, 0.0f);
}

TEST(LightingTest, TorchLightsUseTheirParams) {
    const std::vector<Torch> torches{{.mount = {0.0f, 0.0f, 1.6f}, .yaw = 0.0f}};
    const TorchLightParams params{.intensity = 2.0f, .range = 3.0f};
    const PointLight light = torch_lights(torches, 1.0f, params)[0];
    EXPECT_FLOAT_EQ(light.intensity, 2.0f * torch_flicker(1.0f, torch_phase(0)));
    EXPECT_FLOAT_EQ(light.range, 3.0f);
}

TEST(LightingTest, SceneLightingPutsThePlayersLightFirst) {
    std::vector<Torch> torches;
    torches.reserve(40);
    for (int i = 0; i < 40; ++i) {
        torches.push_back({.mount = {static_cast<float>(i), 0.0f, 1.6f}, .yaw = 0.0f});
    }
    LightSettings settings;
    settings.ambient = glm::vec3(0.2f);
    settings.player_intensity = 7.0f;
    settings.player_range = 9.0f;
    const glm::vec3 eyes{0.5f, 0.5f, 1.7f};
    const Lighting lighting = scene_lighting(settings, eyes, torches, std::nullopt, 1.0f, {0.0f, 0.0f, 0.0f});
    EXPECT_EQ(lighting.ambient, glm::vec3(0.2f));
    ASSERT_EQ(lighting.lights.size(), kMaxLights);
    EXPECT_EQ(lighting.lights[0].position, eyes);
    EXPECT_EQ(lighting.lights[0].color, settings.player_color);
    EXPECT_FLOAT_EQ(lighting.lights[0].intensity, 7.0f);
    EXPECT_FLOAT_EQ(lighting.lights[0].range, 9.0f);
    EXPECT_FALSE(lighting.lights[0].shadow_id.has_value());
    // The torches nearest to the viewer come next.
    EXPECT_EQ(lighting.lights[1].shadow_id, 0U);
    EXPECT_EQ(lighting.lights[2].shadow_id, 1U);
}

TEST(LightingTest, SceneLightingKeepsMaxLightsInRange) {
    const std::vector<Torch> torches(5, Torch{.mount = {1.0f, 0.0f, 1.6f}, .yaw = 0.0f});
    LightSettings settings;
    settings.max_lights = 3;
    EXPECT_EQ(scene_lighting(settings, {}, torches, std::nullopt, 0.0f, {}).lights.size(), 3U);
    settings.max_lights = 0;  // Counts as 1: only the player's light.
    EXPECT_EQ(scene_lighting(settings, {}, torches, std::nullopt, 0.0f, {}).lights.size(), 1U);
    settings.max_lights = kMaxLights + 10;
    EXPECT_EQ(scene_lighting(settings, {}, torches, std::nullopt, 0.0f, {}).lights.size(), 6U);
}

TEST(LightingTest, ExitPulsesSlowlyBetweenPeakAndLow) {
    EXPECT_FLOAT_EQ(exit_pulse(0.0f), 1.0f);
    EXPECT_NEAR(exit_pulse(kExitPulsePeriod / 2.0f), 0.6f, 1e-5f);
    EXPECT_NEAR(exit_pulse(kExitPulsePeriod), 1.0f, 1e-5f);
    for (int step = 0; step < 1000; ++step) {
        const float time = static_cast<float>(step) * 0.01f;
        EXPECT_GE(exit_pulse(time), 0.6f - 1e-5f);
        EXPECT_LE(exit_pulse(time), 1.0f + 1e-5f);
    }
}

TEST(LightingTest, ExitLightIsInCenterOfCubeAndCastsShadows) {
    const LevelExit exit{.floor_point = {1.0f, 2.0f, 3.3f}};
    const PointLight light = exit_light(exit, 0.0f, {.intensity = 4.0f, .range = 7.0f});
    EXPECT_EQ(light.position, glm::vec3(1.0f, 2.0f, 3.3f + LevelExit::kHoverHeight + (LevelExit::kSize / 2.0f)));
    EXPECT_EQ(light.color, kExitLightColor);
    EXPECT_FLOAT_EQ(light.intensity, 4.0f);
    EXPECT_FLOAT_EQ(light.range, 7.0f);
    EXPECT_EQ(light.shadow_id, kExitShadowId);
    EXPECT_NEAR(exit_light(exit, kExitPulsePeriod / 2.0f).intensity, 0.6f * kExitIntensity, 1e-5f);
}

TEST(LightingTest, SceneLightingChoosesExitLightAmongTorches) {
    const std::vector<Torch> torches(3, Torch{.mount = {10.0f, 0.0f, 1.6f}, .yaw = 0.0f});
    const LevelExit exit{.floor_point = {1.0f, 0.0f, 0.0f}};
    LightSettings settings;
    settings.max_lights = 2;
    // The exit is nearest to the viewer, so it is the only light chosen next to the player's.
    const Lighting lighting = scene_lighting(settings, {}, torches, exit, 0.0f, {});
    ASSERT_EQ(lighting.lights.size(), 2U);
    EXPECT_EQ(lighting.lights[1].shadow_id, kExitShadowId);
    settings.max_lights = kMaxLights;
    EXPECT_EQ(scene_lighting(settings, {}, torches, exit, 0.0f, {}).lights.size(), 5U);
}

}  // namespace
}  // namespace manifold
