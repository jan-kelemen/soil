#include <heightmap.hpp>

#include <cppext_numeric.hpp>

#include <stb_image.h>

#include <cassert>
#include <cstdint>

soil::heightmap::heightmap(std::filesystem::path const& path)
{
    int width; // NOLINT
    int height; // NOLINT
    int channels; // NOLINT
    stbi_uc* const pixels{stbi_load(path.generic_string().c_str(),
        &width,
        &height,
        &channels,
        STBI_grey)};

    assert(width == height);
    assert(pixels);

    dimension_ = cppext::narrow<size_t>(width);
    data_.reserve(dimension_ * dimension_);

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    for (auto point :
        std::span{reinterpret_cast<uint8_t*>(pixels), dimension_ * dimension_})
    {
        data_.push_back(cppext::as_fp(point));
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    stbi_image_free(pixels);
}

size_t soil::heightmap::dimension() const { return dimension_; }

std::span<float const> soil::heightmap::data() const { return data_; }
