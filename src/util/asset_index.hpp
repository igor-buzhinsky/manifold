#pragma once

#include <cstdint>

namespace manifold {

/// The number of an asset among those of its key in the asset mapping: 0 for the first file listed for the key, see
/// AssetMapping. Game objects keep these numbers instead of file names, e.g. Door::sounds, so that they don't depend
/// on where the files are.
using AssetIndex = std::uint32_t;

}  // namespace manifold
