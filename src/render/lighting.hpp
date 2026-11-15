#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "world/torch.hpp"
#include "world/world.hpp"

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

/// Color of the light of a torch's flame, linear RGB.
///
/// Orange is a warm fire. Blue and green are colder and feel wrong: a deep blue that leans to indigo, and
/// a sickly green that leans to yellow.
glm::vec3 torch_light_color(TorchFlame flame);
/// Brightness of a torch at 1 m, before flicker.
inline constexpr float kTorchIntensity = 3.5f;
/// Range of a torch's light, in metres.
inline constexpr float kTorchRange = 6.0f;

/// How bright and how far the lights of torches shine.
struct TorchLightParams {
    float intensity = kTorchIntensity;  ///< Brightness at 1 m, before flicker.
    float range = kTorchRange;          ///< In metres.
};

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

/// The light of a torch at a moment, with flicker, at Torch::light_position(), in the color of its flame.
///
/// The light stays in place, because its shadow map is drawn once. Its `shadow_id` is not set.
///
/// \param torch  The torch.
/// \param time   Seconds since the start of the game.
/// \param phase  Radians, see torch_phase().
/// \param params How bright and how far the light shines.
PointLight torch_light(const Torch& torch, float time, float phase, const TorchLightParams& params = {});

/// The lights of all torches at a moment, in the same order as the torches.
///
/// They cast shadows: the light of torch i has `shadow_id` i.
///
/// \param torches The torches. Torch i flickers with torch_phase(i).
/// \param time    Seconds since the start of the game.
/// \param params  How bright and how far the lights shine.
std::vector<PointLight> torch_lights(std::span<const Torch> torches, float time, const TorchLightParams& params = {});

/// Color of the light of the level exit, linear RGB: blue, a little lighter than its cube.
inline constexpr glm::vec3 kExitLightColor{0.15f, 0.35f, 1.0f};
/// Brightness of the exit's light at 1 m, at the peak of its pulse.
inline constexpr float kExitIntensity = 3.0f;
/// Range of the exit's light, in metres.
inline constexpr float kExitRange = 5.0f;
/// The shadow map id of the exit's light. Torch lights use their index, see torch_lights(), which never gets
/// this large.
inline constexpr std::uint32_t kExitShadowId = std::numeric_limits<std::uint32_t>::max();

/// Length of one pulse of the exit's light, in seconds.
inline constexpr float kExitPulsePeriod = 2.0f;

/// How bright and how far the light of the level exit shines.
struct ExitLightParams {
    float intensity = kExitIntensity;  ///< Brightness at 1 m, at the peak of the pulse.
    float range = kExitRange;          ///< In metres.
};

/// How bright the slowly pulsing light of the exit is at a moment, relative to its peak.
///
/// \param time Seconds since the start of the game.
/// \return A factor in [0.6, 1], which goes from the peak down and back once every kExitPulsePeriod.
float exit_pulse(float time);

/// The light of the level exit at a moment: at the center of its cube, pulsing, see exit_pulse().
///
/// It casts shadows, with kExitShadowId. The cube itself casts none, so it doesn't block its own light.
///
/// \param exit   The exit.
/// \param time   Seconds since the start of the game.
/// \param params How bright and how far the light shines.
PointLight exit_light(const LevelExit& exit, float time, const ExitLightParams& params = {});

/// Distance over which lights fade out before they are left out by select_lights(), in metres.
inline constexpr float kLightFadeDistance = 2.0f;

/// Chooses the lights that matter most to a viewer, and fades out those that are about to be left out.
///
/// Lights are ordered by their gap: their distance from the viewer minus their range, so the nearest come
/// first. Lights with the same gap keep their order. The first `max_count` lights are chosen.
///
/// If some lights are left out, the gap of the first of them is the cut. A chosen light's intensity is
/// multiplied by (cut - gap) / fade_distance, clamped to [0, 1]. So a light fades out as its gap approaches
/// the cut, and has none of its intensity left when another light takes its place, which comes in from
/// zero. As the viewer moves, the intensity of every light changes smoothly, without popping.
///
/// \param lights        All lights to choose from.
/// \param viewer        Where the scene is seen from.
/// \param max_count     The most lights to return.
/// \param fade_distance How far from the cut lights start to fade out, in metres. Values below 1 mm count
///                      as 1 mm, which avoids dividing by zero.
/// \return At most `max_count` lights, nearest first.
std::vector<PointLight> select_lights(std::span<const PointLight> lights, const glm::vec3& viewer,
                                      std::size_t max_count, float fade_distance = kLightFadeDistance);

/// The lights of a scene, which the debug overlay can change while the game runs.
struct LightSettings {
    glm::vec3 ambient{0.0f};  ///< See Lighting::ambient. Each level sets it, see level_ambient().
    /// Color of the light spell: a light at the player's eyes, linear RGB.
    glm::vec3 player_color{1.0f, 0.85f, 0.6f};
    float player_intensity = 5.0f;  ///< Brightness of the player's light at 1 m.
    float player_range = 15.0f;     ///< Range of the player's light, in metres.
    TorchLightParams torch;
    ExitLightParams exit;
    float fade_distance = kLightFadeDistance;  ///< See select_lights().
    /// Most lights drawn at once, including the player's, from 1 to kMaxLights. Fewer show how lights
    /// fade out, see select_lights().
    std::size_t max_lights = kMaxLights;
};

/// Luminance of the ambient light of the levels that have it, see level_ambient().
inline constexpr float kLevelAmbientLuminance = 0.1f;

/// The ambient light of a level: none for half of the seeds, and for the others light of the hue of the level's
/// texture, so that the level seems lit without a visible source.
///
/// The light has the color of the texture scaled to a luminance of kLevelAmbientLuminance, so that light and dark
/// textures give equally bright light. A black texture gives grey light.
///
/// \param level_seed    The seed of the level. The choice depends only on it.
/// \param texture_color The mean color of the level's texture, linear RGB, see mean_linear_color().
/// \return The ambient light, linear RGB.
glm::vec3 level_ambient(std::uint64_t level_seed, const glm::vec3& texture_color);

/// The light in the scene at a moment: the ambient light, the player's light, and the lights of the torches
/// and the exit that matter most to the viewer.
///
/// \param settings The lights. A `max_lights` outside [1, kMaxLights] counts as the nearest limit.
/// \param eyes     Where the player's light is: at the player's eyes.
/// \param torches  The torches, see torch_lights().
/// \param exit     The level exit, see exit_light(), or none.
/// \param time     Seconds since the start of the game.
/// \param viewer   Where the scene is seen from, see select_lights().
/// \return The player's light first, then the chosen lights of the torches and the exit, nearest first.
Lighting scene_lighting(const LightSettings& settings, const glm::vec3& eyes, std::span<const Torch> torches,
                        const std::optional<LevelExit>& exit, float time, const glm::vec3& viewer);

}  // namespace manifold
