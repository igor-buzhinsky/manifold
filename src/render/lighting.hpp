#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "world/torch.hpp"

namespace manifold {

/// A light that shines equally in all directions from one point.
///
/// Its brightness falls off with the square of the distance, and fades out smoothly to zero at `range`.
struct PointLight {
    glm::vec3 position{0.0f};  ///< In world coordinates.
    glm::vec3 color{1.0f};     ///< Linear RGB.
    /// Brightness at a distance of 1 m, before the fade towards `range`.
    float intensity = 1.0f;
    float range = 10.0f;  ///< Farther than this, in metres, the light has no effect.
    /// If set, the light casts shadows, and this identifies its shadow map. The light must not move, and
    /// different lights must have different ids, because the renderer keeps each shadow map for later
    /// frames.
    std::optional<std::uint32_t> shadow_id = std::nullopt;
};

/// The most point lights that are drawn at once.
inline constexpr std::size_t kMaxLights = 32;

/// All light in the scene.
struct Lighting {
    glm::vec3 ambient{0.0f};         ///< Light that reaches every surface from all directions, linear RGB.
    std::vector<PointLight> lights;  ///< Only the first kMaxLights are used.
};

/// Color of torch light, linear RGB: a warm orange.
inline constexpr glm::vec3 kTorchColor{1.0f, 0.55f, 0.2f};
/// Brightness of a torch at 1 m, before flicker.
inline constexpr float kTorchIntensity = 3.5f;
/// Range of a torch's light, in metres.
inline constexpr float kTorchRange = 6.0f;

/// The phase of a torch's flicker, so that neighboring torches don't flicker in step.
///
/// \param index Index of the torch in the level.
/// \return Radians in [0, 2 pi).
float torch_phase(std::size_t index);

/// How bright a flickering flame is at a moment, relative to its full brightness.
///
/// The result is a sum of sines of unrelated frequencies, so it doesn't visibly repeat. It changes
/// smoothly with time.
///
/// \param time  Seconds since the start of the game.
/// \param phase Radians, see torch_phase().
/// \return A factor in [0.75, 1].
float torch_flicker(float time, float phase);

/// The light of a torch at a moment, with flicker, at Torch::light_position().
///
/// The light stays in place, because its shadow map is drawn once. Its `shadow_id` is not set.
///
/// \param torch The torch.
/// \param time  Seconds since the start of the game.
/// \param phase Radians, see torch_phase().
PointLight torch_light(const Torch& torch, float time, float phase);

/// The lights of all torches at a moment, in the same order as the torches.
///
/// They cast shadows: the light of torch i has `shadow_id` i.
///
/// \param torches The torches. Torch i flickers with torch_phase(i).
/// \param time    Seconds since the start of the game.
std::vector<PointLight> torch_lights(std::span<const Torch> torches, float time);

/// Distance over which lights fade out before they are left out by select_lights(), in metres.
inline constexpr float kLightFadeDistance = 2.0f;

/// Chooses the lights that matter most to a viewer, and fades out those that are about to be left out.
///
/// Lights are ordered by their gap: their distance from the viewer minus their range, so the nearest come
/// first. Lights with the same gap keep their order. The first `max_count` lights are chosen.
///
/// If some lights are left out, the gap of the first of them is the cut. A chosen light's intensity is
/// multiplied by (cut - gap) / fade_distance, clamped to [0, 1]. So a light fades out as it moves towards
/// the cut, and has none of its intensity left when another light takes its place, which comes in from
/// zero. As the viewer moves, the intensity of every light changes smoothly, without popping.
///
/// \param lights        All lights to choose from.
/// \param viewer        Where the scene is seen from.
/// \param max_count     The most lights to return.
/// \param fade_distance How far from the cut lights start to fade out, in metres. Must be positive.
/// \return At most `max_count` lights, nearest first.
std::vector<PointLight> select_lights(std::span<const PointLight> lights, const glm::vec3& viewer,
                                      std::size_t max_count, float fade_distance = kLightFadeDistance);

}  // namespace manifold
