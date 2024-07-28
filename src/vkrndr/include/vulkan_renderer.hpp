#ifndef VKRNDR_VULKAN_RENDERER_INCLUDED
#define VKRNDR_VULKAN_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vulkan/vulkan_core.h>

#include <gltf_manager.hpp>
#include <vulkan_font.hpp>
#include <vulkan_image.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace vkrndr
{
    class font_manager;
    class imgui_render_layer;
    struct vulkan_buffer;
    struct vulkan_context;
    struct vulkan_device;
    class vulkan_scene;
    class vulkan_swap_chain;
    class vulkan_window;
    struct vulkan_queue;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] vulkan_renderer final
    {
    public: // Construction
        vulkan_renderer(vulkan_window* window,
            vulkan_context* context,
            vulkan_device* device);

        vulkan_renderer(vulkan_renderer const&) = delete;

        vulkan_renderer(vulkan_renderer&&) noexcept = delete;

    public: // Destruction
        ~vulkan_renderer();

    public: // Interface
        [[nodiscard]] constexpr VkDescriptorPool
        descriptor_pool() const noexcept;

        [[nodiscard]] VkFormat image_format() const;

        [[nodiscard]] uint32_t image_count() const;

        [[nodiscard]] VkExtent2D extent() const;

        [[nodiscard]] bool imgui_layer() const;

        void imgui_layer(bool state);

        [[nodiscard]] bool begin_frame(vulkan_scene* scene);

        void end_frame();

        void draw(vulkan_scene* scene);

        [[nodiscard]] vulkan_image load_texture(
            std::filesystem::path const& texture_path,
            VkFormat format);

        [[nodiscard]] vulkan_image transfer_image(
            std::span<std::byte const> image_data,
            VkExtent2D extent,
            VkFormat format,
            uint32_t mip_levels);

        void transfer_buffer(vulkan_buffer const& source,
            vulkan_buffer const& target);

        [[nodiscard]] vulkan_font
        load_font(std::filesystem::path const& font_path, uint32_t font_size);

        [[nodiscard]] std::unique_ptr<vkrndr::gltf_model> load_model(
            std::filesystem::path const& model_path);

    public: // Operators
        vulkan_renderer& operator=(vulkan_renderer const&) = delete;

        vulkan_renderer& operator=(vulkan_renderer&&) noexcept = delete;

    private: // Types
        struct [[nodiscard]] frame_data final
        {
            vulkan_queue* present_queue{};
            VkCommandPool present_command_pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> present_command_buffers;
            size_t used_present_command_buffers_{};

            vulkan_queue* transfer_queue{};
            VkCommandPool transfer_command_pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> transfer_command_buffers;
            size_t used_transfer_command_buffers{};
        };

    private:
        [[nodiscard]] VkCommandBuffer request_command_buffer(
            bool transfer_only);

    private: // Data
        vulkan_window* window_;
        vulkan_context* context_;
        vulkan_device* device_;

        std::unique_ptr<vulkan_swap_chain> swap_chain_;

        cppext::cycled_buffer<frame_data> frame_data_;

        VkDescriptorPool descriptor_pool_{};

        std::unique_ptr<imgui_render_layer> imgui_layer_;
        std::unique_ptr<font_manager> font_manager_;
        std::unique_ptr<gltf_manager> gltf_manager_;

        uint32_t image_index_{};
    };
} // namespace vkrndr

constexpr VkDescriptorPool
vkrndr::vulkan_renderer::descriptor_pool() const noexcept
{
    return descriptor_pool_;
}

#endif // !VKRNDR_VULKAN_RENDERER_INCLUDED
