#ifndef SOIL_HEIGHTMAP_INCLUDED
#define SOIL_HEIGHTMAP_INCLUDED

#include <glm/vec3.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

class btCollisionShape;

namespace soil
{
    class [[nodiscard]] heightmap final
    {
    public:
        explicit heightmap(size_t dimension);

        heightmap(heightmap const&) = default;

        heightmap(heightmap&&) noexcept = default;

    public:
        ~heightmap() = default;

    public:
        [[nodiscard]] size_t dimension() const;

        [[nodiscard]] glm::vec3 const& scaling() const;

        // cppcheck-suppress returnByReference
        [[nodiscard]] std::span<float const> data() const;

        [[nodiscard]] float value(size_t x, size_t y) const
        {
            return data_[y * dimension_ + x];
        }

        [[nodiscard]] std::unique_ptr<btCollisionShape> collision_shape() const;

    private:
        heightmap& operator=(heightmap const&) = default;

        heightmap& operator=(heightmap&&) noexcept = default;

    private:
        size_t dimension_;
        glm::vec3 scaling_;
        std::vector<float> data_;
    };
} // namespace soil

#endif
