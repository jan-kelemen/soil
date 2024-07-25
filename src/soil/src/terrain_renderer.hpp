#ifndef SOIL_TERRAIN_RENDERER_INCLUDED
#define SOIL_TERRAIN_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_scene.hpp>

#include <vulkan/vulkan_core.h>

#include <memory>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_pipeline;
    class vulkan_renderer;
    class camera;
} // namespace vkrndr

namespace soil
{
    class heightmap;
} // namespace soil

namespace soil
{
    class [[nodiscard]] terrain_renderer : public vkrndr::vulkan_scene
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
        ~terrain_renderer() override;

        void resize(VkExtent2D extent) override;

        void update(vkrndr::camera const& camera, float delta_time) override;

        void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) override;

        void draw_imgui() override;

    public:
        terrain_renderer& operator=(terrain_renderer const&) = delete;

        terrain_renderer& operator=(terrain_renderer&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_resources final
        {
            vkrndr::vulkan_buffer vertex_uniform;
            vkrndr::mapped_memory uniform_map{};
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private:
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;
        vkrndr::vulkan_image* color_image_;
        vkrndr::vulkan_image* depth_buffer_;

        vkrndr::vulkan_image texture_;
        vkrndr::vulkan_image texture_normal_;

        VkSampler texture_sampler_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        std::unique_ptr<vkrndr::vulkan_pipeline> pipeline_;

        vkrndr::vulkan_buffer vertex_index_buffer_;
        uint32_t vertex_count_{};
        VkDeviceSize index_offset_{};
        uint32_t index_count_{};

        cppext::cycled_buffer<frame_resources> frame_data_;
    };
} // namespace soil

#endif
