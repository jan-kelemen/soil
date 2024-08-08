#ifndef SOIL_TERRAIN_INCLUDED
#define SOIL_TERRAIN_INCLUDED

#include <terrain_renderer.hpp>

#include <vulkan_buffer.hpp>

#include <entt/entt.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_image;
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class heightmap;
    class physics_engine;
    class perspective_camera;
} // namespace soil

namespace soil
{
    class [[nodiscard]] terrain final
    {
    public:
        terrain(heightmap const& heightmap,
            physics_engine* physics_engine,
            vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer,
            vkrndr::vulkan_image* color_image,
            vkrndr::vulkan_image* depth_buffer);

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
        physics_engine* physics_engine_;
        vkrndr::vulkan_device* device_;

        entt::registry chunk_registry_;

        uint32_t terrain_dimension_;
        uint32_t chunk_dimension_;

        terrain_renderer renderer_;

        int lod_{};
    };
} // namespace soil

#endif
