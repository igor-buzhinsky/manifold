#include "render/shadow_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace manifold {

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
