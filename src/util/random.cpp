#include "util/random.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace manifold {

namespace {

/// A uniformly random number in [0, range), without bias. `range` must be positive. It is 64 bits wide on every
/// platform, unlike std::size_t, so random_int() covers the whole range of int also where std::size_t has 32 bits.
std::uint64_t random_below(Rng& rng, std::uint64_t range) {
    // Numbers at and above the largest multiple of `range` are drawn again, so that every index is equally likely.
    // At most half of the numbers are drawn again, and for small counts almost none.
    const std::uint64_t limit =
        std::numeric_limits<std::uint64_t>::max() - (std::numeric_limits<std::uint64_t>::max() % range);
    std::uint64_t number = rng();
    while (number >= limit) {
        number = rng();
    }
    return number % range;
}

}  // namespace

std::size_t random_index(Rng& rng, std::size_t count) {
    if (count == 0) {
        throw std::invalid_argument("random_index needs at least one choice");
    }
    return static_cast<std::size_t>(random_below(rng, count));
}

int random_int(Rng& rng, int min, int max) {
    if (min > max) {
        throw std::invalid_argument("random_int needs min <= max");
    }
    const auto count = static_cast<std::uint64_t>(static_cast<std::int64_t>(max) - static_cast<std::int64_t>(min)) + 1U;
    return static_cast<int>(static_cast<std::int64_t>(min) + static_cast<std::int64_t>(random_below(rng, count)));
}

double random_unit(Rng& rng) {
    // The top 53 bits fill the mantissa of a double exactly.
    constexpr int kBits = std::numeric_limits<double>::digits;
    return static_cast<double>(rng() >> (64 - kBits)) * std::ldexp(1.0, -kBits);
}

float random_float(Rng& rng, float min, float max) {
    if (!std::isfinite(min) || !std::isfinite(max) || min > max) {
        throw std::invalid_argument("random_float needs finite min <= max");
    }
    if (min == max) {
        return min;
    }
    // std::fma rounds once, as IEEE 754 requires, on every platform. A plain `min + (max - min) * u` may be rounded
    // once or twice, depending on whether the compiler fuses it into an FMA instruction, e.g. on arm64 but not on
    // x86-64, so its last bit could differ between platforms.
    return static_cast<float>(
        std::fma(static_cast<double>(max) - static_cast<double>(min), random_unit(rng), static_cast<double>(min)));
}

bool random_chance(Rng& rng, double probability) { return random_unit(rng) < probability; }

std::size_t random_weighted_index(Rng& rng, std::span<const double> weights) {
    double total = 0.0;
    for (const double weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0) {
            throw std::invalid_argument("random_weighted_index needs finite, non-negative weights");
        }
        total += weight;
    }
    if (!(total > 0.0)) {
        throw std::invalid_argument("random_weighted_index needs a positive weight");
    }
    const double target = random_unit(rng) * total;
    double sum = 0.0;
    std::size_t last_positive = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        if (weights[i] <= 0.0) {
            continue;
        }
        sum += weights[i];
        if (target < sum) {
            return i;
        }
        last_positive = i;
    }
    // Rounding can leave the sum just below `target`; the last weight that can be chosen takes it.
    return last_positive;
}

}  // namespace manifold
