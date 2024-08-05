#ifndef SOIL_TERRAIN_RENDERER_INCLUDED
#define SOIL_TERRAIN_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_render_pass.hpp>
#include <vulkan_buffer.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_pipeline;
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class perspective_camera;
} // namespace soil

namespace soil
{
    struct [[nodiscard]] terrain_vertex final
    {
        glm::uvec2 position;
    };

    class [[nodiscard]] terrain_renderer final
    {
    public:
        terrain_renderer(vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer,
            vkrndr::vulkan_image* color_image,
            vkrndr::vulkan_image* depth_buffer,
            vkrndr::vulkan_buffer* heightmap_buffer,
            uint32_t terrain_dimension,
            uint32_t chunk_dimension);

        terrain_renderer(terrain_renderer const&) = delete;

        terrain_renderer(terrain_renderer&&) noexcept = delete;

    public:
        ~terrain_renderer();

    public:
        [[nodiscard]] int lod_levels() const
        {
            return index_buffers_.back().lod;
        }

        void update(soil::perspective_camera const& camera);

        vkrndr::render_pass_guard begin_render_pass(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkRect2D render_area);

        void draw(VkCommandBuffer command_buffer,
            uint32_t lod,
            uint32_t chunk_index,
            vkrndr::vulkan_buffer* vertex_buffer,
            glm::mat4 const& model);

        void end_render_pass();

        void draw_imgui();

    public:
        terrain_renderer& operator=(terrain_renderer const&) = delete;

        terrain_renderer& operator=(terrain_renderer&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_resources final
        {
            vkrndr::vulkan_buffer camera_uniform;
            vkrndr::mapped_memory camera_uniform_map{};
            vkrndr::vulkan_buffer chunk_uniform;
            vkrndr::mapped_memory chunk_uniform_map{};
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};

            vkrndr::vulkan_buffer* last_bound_vertex_buffer{};
            uint32_t current_chunk_;
        };

        struct [[nodiscard]] lod_index_buffer final
        {
            uint32_t lod;
            uint32_t index_count;
            vkrndr::vulkan_buffer index_buffer;
        };

    private:
        void fill_index_buffer(uint32_t dimension, uint32_t lod);

    private:
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;
        vkrndr::vulkan_image* color_image_;
        vkrndr::vulkan_image* depth_buffer_;

        uint32_t terrain_dimension_;
        uint32_t chunk_dimension_;
        uint32_t chunks_per_dimension_;

        std::vector<lod_index_buffer> index_buffers_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        std::unique_ptr<vkrndr::vulkan_pipeline> pipeline_;

        cppext::cycled_buffer<frame_resources> frame_data_;
    };
} // namespace soil

#endif
