#include "level/texture_kind.hpp"

namespace manifold {

const char* texture_kind_name(TextureKind kind) {
    switch (kind) {
        case TextureKind::kGrass:
            return "grass";
        case TextureKind::kWood:
            return "wood";
        case TextureKind::kMetal:
            return "metal";
        case TextureKind::kStone:
            return "stone";
    }
    return "unknown";
}

}  // namespace manifold
