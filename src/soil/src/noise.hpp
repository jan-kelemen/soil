#ifndef SOIL_NOISE_INCLUDED
#define SOIL_NOISE_INCLUDED

#include <cstddef>
#include <span>

namespace soil
{
    void generate_2d_noise(std::span<std::byte> output, size_t dimension);
} // namespace soil

#endif
