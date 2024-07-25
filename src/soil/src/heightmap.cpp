#include <heightmap.hpp>

#include <bullet_adapter.hpp>

#include <cppext_numeric.hpp>

#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <PerlinNoise.hpp>

#include <memory>

soil::heightmap::heightmap(size_t dimension)
    : dimension_{dimension}
    , scaling_{10.0f, 5.0f, 10.0f}
{
    data_.reserve(dimension_ * dimension_);

    constexpr siv::PerlinNoise::seed_type seed{123456u};

    siv::BasicPerlinNoise<float> const perlin{seed};
    for (size_t y{}; y != dimension_; ++y)
    {
        for (size_t x{}; x != dimension_; ++x)
        {
            if (y != dimension_ / 2 || x != dimension_ / 2)
            {
                data_.push_back(
                    perlin.noise2D_01(cppext::as_fp(x), cppext::as_fp(y)));
            }
            else
            {
                data_.push_back(0.0f);
            }
        }
    }
}

size_t soil::heightmap::dimension() const { return dimension_; }

glm::vec3 const& soil::heightmap::scaling() const { return scaling_; }

std::span<float const> soil::heightmap::data() const { return data_; }

std::unique_ptr<btCollisionShape> soil::heightmap::collision_shape() const
{
    auto heightfield_shape{std::make_unique<btHeightfieldTerrainShape>(
        cppext::narrow<int>(dimension_),
        cppext::narrow<int>(dimension_),
        data_.data(),
        1.0f,
        0.0f,
        1.0f,
        1,
        PHY_FLOAT,
        false)};
    heightfield_shape->setLocalScaling(soil::to_bullet(scaling_));

    return heightfield_shape;
}
