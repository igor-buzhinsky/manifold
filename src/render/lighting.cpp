#include "render/lighting.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

glm::vec3 torch_light_color(TorchFlame flame) {
    switch (flame) {
        case TorchFlame::kOrange:
            return {1.0f, 0.55f, 0.2f};
        case TorchFlame::kBlue:
            return {0.18f, 0.24f, 1.0f};
        case TorchFlame::kGreen:
            return {0.45f, 1.0f, 0.2f};
    }
    return glm::vec3(1.0f);
}

float torch_phase(std::size_t index) {
    // Multiples of 1 / phi = phi - 1, the inverse of the golden ratio, spread out evenly over the circle,
    // however many there are.
    const double fraction = std::fmod(static_cast<double>(index) * (std::numbers::phi - 1.0), 1.0);
    return static_cast<float>(fraction * 2.0 * std::numbers::pi);
}

float torch_flicker(float time, float phase) {
    // The weights add up to 1, so `wave` is in [-1, 1].
    const float wave = (0.5f * std::sin((9.1f * time) + phase)) + (0.3f * std::sin((15.7f * time) + (1.7f * phase))) +
                       (0.2f * std::sin((23.3f * time) + (2.9f * phase)));
    return 0.875f + (0.125f * wave);
}

PointLight torch_light(const Torch& torch, float time, float phase, const TorchLightParams& params) {
    return {.position = torch.light_position(),
            .color = torch_light_color(torch.flame),
            .intensity = params.intensity * torch_flicker(time, phase),
            .range = params.range};
}

std::vector<PointLight> torch_lights(std::span<const Torch> torches, float time, const TorchLightParams& params) {
    std::vector<PointLight> lights;
    lights.reserve(torches.size());
    for (std::size_t i = 0; i < torches.size(); ++i) {
        PointLight light = torch_light(torches[i], time, torch_phase(i), params);
        light.shadow_id = static_cast<std::uint32_t>(i);
        lights.push_back(light);
    }
    return lights;
}

float exit_pulse(float time) {
    // A cosine starts at the peak. Wrapping the time keeps the argument small, where it is precise.
    const float angle = 2.0f * std::numbers::pi_v<float> * std::fmod(time, kExitPulsePeriod) / kExitPulsePeriod;
    return 0.8f + (0.2f * std::cos(angle));
}

PointLight exit_light(const LevelExit& exit, float time, const ExitLightParams& params) {
    return {.position = exit.center(),
            .color = kExitLightColor,
            .intensity = params.intensity * exit_pulse(time),
            .range = params.range,
            .shadow_id = kExitShadowId};
}

std::vector<PointLight> select_lights(std::span<const PointLight> lights, const glm::vec3& viewer,
                                      std::size_t max_count, float fade_distance) {
    const auto gap = [&viewer](const PointLight& light) { return glm::distance(light.position, viewer) - light.range; };
    std::vector<PointLight> selected(lights.begin(), lights.end());
    std::ranges::stable_sort(selected, {}, gap);
    if (selected.size() <= max_count) {
        return selected;
    }
    constexpr float kMinFadeDistance = 0.001f;
    const float fade = std::max(fade_distance, kMinFadeDistance);
    const float cut = gap(selected[max_count]);
    selected.resize(max_count);
    for (PointLight& light : selected) {
        light.intensity *= std::clamp((cut - gap(light)) / fade, 0.0f, 1.0f);
    }
    return selected;
}

Lighting scene_lighting(const LightSettings& settings, const glm::vec3& eyes, std::span<const Torch> torches,
                        const std::optional<LevelExit>& exit, float time, const glm::vec3& viewer) {
    Lighting lighting{
        .ambient = settings.ambient,
        .lights = {{.position = eyes,
                    .color = settings.player_color,
                    .intensity = settings.player_intensity,
                    .range = settings.player_range}},
    };
    const std::size_t max_lights = std::clamp<std::size_t>(settings.max_lights, 1, kMaxLights);
    std::vector<PointLight> candidates = torch_lights(torches, time, settings.torch);
    if (exit) {
        candidates.push_back(exit_light(*exit, time, settings.exit));
    }
    const std::vector<PointLight> chosen = select_lights(candidates, viewer, max_lights - 1, settings.fade_distance);
    lighting.lights.insert(lighting.lights.end(), chosen.begin(), chosen.end());
    return lighting;
}

}  // namespace manifold
