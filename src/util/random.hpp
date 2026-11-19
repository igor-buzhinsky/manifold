#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <ranges>
#include <span>

namespace manifold {

// Random choices that are the same on every platform.
//
// The C++ standard fixes the output of the random engines, such as std::mt19937_64, for each seed, but not the
// algorithms of the distributions (std::uniform_int_distribution, std::uniform_real_distribution,
// std::bernoulli_distribution, std::discrete_distribution, ...) or of std::shuffle and std::sample: libstdc++, libc++
// and the MSVC library each turn the engine's numbers into results in their own way. A seed must give the same level,
// texture, sounds and party everywhere, e.g. so that a save made on one platform loads the same level on another, so
// the game makes all its random choices with the functions here, from the raw output of Rng. See CODING_STYLE.md.

/// The random number generator of the game. Its output for each seed is the same everywhere.
using Rng = std::mt19937_64;

/// A uniformly random index in [0, count), without bias.
///
/// \throws std::invalid_argument if `count` is 0.
std::size_t random_index(Rng& rng, std::size_t count);

/// A uniformly random integer in [min, max], both included, without bias.
///
/// \throws std::invalid_argument if `min` > `max`.
int random_int(Rng& rng, int min, int max);

/// A uniformly random number in [0, 1), with 53 random bits.
double random_unit(Rng& rng);

/// A uniformly random number from `min` to `max`. Rounding to float can give `max`. Equal bounds give `min`.
///
/// \throws std::invalid_argument if `min` > `max`, or either is not finite.
float random_float(Rng& rng, float min, float max);

/// True with the chance `probability`: never for 0 or less, always for 1 or more.
bool random_chance(Rng& rng, double probability);

/// A random index into `weights`, each chosen with a chance proportional to its weight. Zero weights are never chosen.
///
/// \throws std::invalid_argument if a weight is negative or not finite, or if no weight is positive.
std::size_t random_weighted_index(Rng& rng, std::span<const double> weights);

/// Puts `items`, such as a std::vector or std::array, into a uniformly random order: a Fisher-Yates shuffle with
/// random_index().
template <std::ranges::random_access_range Range>
void random_shuffle(Rng& rng, Range& items) {
    const auto begin = std::ranges::begin(items);
    for (auto i = static_cast<std::size_t>(std::ranges::distance(items)); i > 1; --i) {
        std::ranges::iter_swap(begin + static_cast<std::ptrdiff_t>(i - 1),
                               begin + static_cast<std::ptrdiff_t>(random_index(rng, i)));
    }
}

}  // namespace manifold
