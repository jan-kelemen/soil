#include <heightmap.hpp>

#include <bullet_adapter.hpp>

#include <cppext_numeric.hpp>

#include <BulletCollision/CollisionShapes/btCollisionShape.h> // IWYU pragma: keep
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <stb_image.h>

#include <cassert>
#include <memory>

// IWYU pragma: no_include <BulletCollision/CollisionShapes/btConcaveShape.h>

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

    static constexpr auto max_value{
        cppext::as_fp(std::numeric_limits<uint8_t>::max())};

    for (auto point :
        std::span{reinterpret_cast<uint8_t*>(pixels), dimension_ * dimension_})
    {
        data_.push_back(cppext::as_fp(point));
    }

    stbi_image_free(pixels);
}

size_t soil::heightmap::dimension() const { return dimension_; }

std::span<float const> soil::heightmap::data() const { return data_; }

std::unique_ptr<btCollisionShape> soil::heightmap::collision_shape() const
{
    auto heightfield_shape{std::make_unique<btHeightfieldTerrainShape>(
        cppext::narrow<int>(dimension_),
        100,
        data_.data(),
        0.0f,
        cppext::as_fp(std::numeric_limits<uint8_t>::max()),
        1,
        false)};

    return heightfield_shape;
}
