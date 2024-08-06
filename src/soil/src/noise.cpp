#include <noise.hpp>

#include <cppext_numeric.hpp>

#include <PerlinNoise.hpp>

#include <cassert>
#include <cmath>

void soil::generate_2d_noise(std::span<std::byte> output,
    size_t const dimension)
{
    constexpr siv::PerlinNoise::seed_type seed{123456u};
    constexpr float div_factor{50.0f};

    assert(output.size() >= dimension * dimension);

    siv::BasicPerlinNoise<float> const perlin{seed};

    for (size_t j{}; j != dimension; ++j)
    {
        for (size_t i{}; i != dimension; ++i)
        {
            output[j * dimension + i] = static_cast<std::byte>(
                round(perlin.noise2D_01(cppext::as_fp(i) / div_factor,
                          cppext::as_fp(j) / div_factor) *
                    255));
        }
    }
}
