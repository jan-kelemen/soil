#ifndef SOIL_HEIGHTMAP_INCLUDED
#define SOIL_HEIGHTMAP_INCLUDED

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace soil
{
    class [[nodiscard]] heightmap final
    {
    public:
        explicit heightmap(std::filesystem::path const& path);

        heightmap(heightmap const&) = default;

        heightmap(heightmap&&) noexcept = default;

    public:
        ~heightmap() = default;

    public:
        [[nodiscard]] size_t dimension() const;

        // cppcheck-suppress returnByReference
        [[nodiscard]] std::span<float const> data() const;

        [[nodiscard]] float value(size_t x, size_t y) const
        {
            return data_[y * dimension_ + x];
        }

    private:
        heightmap& operator=(heightmap const&) = default;

        heightmap& operator=(heightmap&&) noexcept = default;

    private:
        size_t dimension_;
        std::vector<float> data_;
    };
} // namespace soil

#endif
