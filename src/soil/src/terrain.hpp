#ifndef SOIL_TERRAIN_INCLUDED
#define SOIL_TERRAIN_INCLUDED

#include <terrain_renderer.hpp>

#include <vulkan_buffer.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <map>
#include <memory>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_image;
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class heightmap;
    class perspective_camera;
} // namespace soil

namespace soil
{
    class [[nodiscard]] terrain final
    {
    public:
        terrain(vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer,
            vkrndr::vulkan_image* color_image,
            vkrndr::vulkan_image* depth_buffer,
            heightmap const& heightmap);

        terrain(terrain const&) = delete;

        terrain(terrain&&) noexcept = delete;

    public:
        ~terrain();

    public:
        void update(soil::perspective_camera const& camera, float delta_time);

        void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkRect2D render_area);

        void draw_imgui();

    public:
        terrain& operator=(terrain const&) = delete;

        terrain& operator=(terrain&&) noexcept = delete;

    private:
        void fill_heightmap(vkrndr::vulkan_renderer* renderer,
            heightmap const& heightmap);

        void fill_vertex_buffer(vkrndr::vulkan_renderer* renderer,
            uint32_t dimension);

    private:
        vkrndr::vulkan_device* device_;

        heightmap const* generator_;
        uint32_t chunk_dimension_;

        uint32_t vertex_count_{};
        vkrndr::vulkan_buffer vertex_buffer_;

        vkrndr::vulkan_buffer heightmap_;

        terrain_renderer renderer_;

        int lod_{};
    };
} // namespace soil

#endif
