#include "render/shadow_map.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/door.hpp"

namespace manifold {

bool light_reaches_door(const glm::vec3& light, float range, const Door& door) {
    // In the door's frame, u runs along the wall and v through the opening, as in Door.
    const glm::vec2 offset = glm::vec2(light - door.base_center);
    const float cos_yaw = std::cos(door.yaw);
    const float sin_yaw = std::sin(door.yaw);
    const glm::vec3 local{(cos_yaw * offset.x) + (sin_yaw * offset.y), (-sin_yaw * offset.x) + (cos_yaw * offset.y),
                          light.z - door.base_center.z};
    // Fully open, the outer edge of a half is half the gap beyond the end of the opening.
    const glm::vec3 low{-(door.width + door.gap()) / 2.0f, -Door::kThickness / 2.0f, 0.0f};
    const glm::vec3 high{(door.width + door.gap()) / 2.0f, Door::kThickness / 2.0f, door.height};
    return glm::distance(local, glm::clamp(local, low, high)) < range;
}

glm::mat4 cube_face_view_projection(const glm::vec3& light, float range, int face) {
    // The direction each face looks in, and its "up", as the OpenGL specification lays out cube map faces.
    static constexpr std::array<glm::vec3, kCubeFaces> kForward{{
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
    }};
    static constexpr std::array<glm::vec3, kCubeFaces> kUp{{
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
    }};
    const auto index = static_cast<std::size_t>(face);
    const glm::mat4 projection = glm::perspective(std::numbers::pi_v<float> / 2.0f, 1.0f, kShadowNearPlane, range);
    return projection * glm::lookAt(light, light + kForward.at(index), kUp.at(index));
}

ShadowCache::ShadowCache(std::size_t slot_count) : slots_(slot_count) {}

ShadowCache::Assignment ShadowCache::assign(std::span<const std::uint32_t> ids) {
    ++request_;
    Assignment result;
    result.slots.resize(ids.size());
    // First, lights that already have a slot keep it, so their shadow maps don't have to be drawn again.
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
            if (slots_[slot].id == ids[i]) {
                slots_[slot].last_used = request_;
                result.slots[i] = slot;
                break;
            }
        }
    }
    // Then the others take the free slot or the one used longest ago. Free slots have last_used = 0.
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (result.slots[i]) {
            continue;
        }
        std::size_t oldest = slots_.size();  // No slot found yet.
        for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
            const bool used_now = slots_[slot].last_used == request_;
            if (!used_now && (oldest == slots_.size() || slots_[slot].last_used < slots_[oldest].last_used)) {
                oldest = slot;
            }
        }
        if (oldest == slots_.size()) {
            break;  // Every slot is used by this request.
        }
        slots_[oldest] = {.id = ids[i], .last_used = request_};
        result.slots[i] = oldest;
        result.to_draw.push_back(oldest);
    }
    return result;
}

}  // namespace manifold
