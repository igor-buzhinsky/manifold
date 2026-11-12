#include "render/lighting.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "world/torch.hpp"

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

PointLight torch_light(const Torch& torch, float time, float phase) {
    return {.position = torch.light_position(),
            .color = torch_light_color(torch.flame),
            .intensity = kTorchIntensity * torch_flicker(time, phase),
            .range = kTorchRange};
}

std::vector<PointLight> torch_lights(std::span<const Torch> torches, float time) {
    std::vector<PointLight> lights;
    lights.reserve(torches.size());
    for (std::size_t i = 0; i < torches.size(); ++i) {
        PointLight light = torch_light(torches[i], time, torch_phase(i));
        light.shadow_id = static_cast<std::uint32_t>(i);
        lights.push_back(light);
    }
    return lights;
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

}  // namespace manifold
