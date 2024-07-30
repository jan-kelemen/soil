#ifndef SOIL_TERRAIN_RENDERER_INCLUDED
#define SOIL_TERRAIN_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <map>
#include <memory>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_pipeline;
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class heightmap;
    class perspective_camera;
} // namespace soil

namespace soil
{
    class [[nodiscard]] terrain_renderer
    {
    public:
        terrain_renderer(vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer,
            vkrndr::vulkan_image* color_image,
            vkrndr::vulkan_image* depth_buffer,
            heightmap const& heightmap);

        terrain_renderer(terrain_renderer const&) = delete;

        terrain_renderer(terrain_renderer&&) noexcept = delete;

    public:
        ~terrain_renderer();

    public:
        void update(soil::perspective_camera const& camera, float delta_time);

        void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkRect2D render_area);

        void draw_imgui();

    public:
        terrain_renderer& operator=(terrain_renderer const&) = delete;

        terrain_renderer& operator=(terrain_renderer&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_resources final
        {
            vkrndr::vulkan_buffer vertex_uniform;
            vkrndr::mapped_memory vertex_uniform_map{};
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private:
        void fill_heightmap(heightmap const& heightmap);

        void fill_vertex_buffer(size_t width, size_t height);

        void fill_index_buffer(size_t width, size_t height, uint32_t lod_step);

    private:
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;
        vkrndr::vulkan_image* color_image_;
        vkrndr::vulkan_image* depth_buffer_;

        uint32_t vertex_count_{};
        vkrndr::vulkan_buffer vertex_buffer_;

        std::map<uint32_t, std::pair<uint32_t, vkrndr::vulkan_buffer>>
            lod_index_buffers_;

        vkrndr::vulkan_buffer heightmap_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        std::unique_ptr<vkrndr::vulkan_pipeline> pipeline_;

        cppext::cycled_buffer<frame_resources> frame_data_;
    };
} // namespace soil

#endif
