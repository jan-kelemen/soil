#ifndef VKRNDR_VULKAN_SCENE_INCLUDED
#define VKRNDR_VULKAN_SCENE_INCLUDED

#include <vkrndr_camera.hpp>

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] vulkan_scene
    {
    public: // Destruction
        virtual ~vulkan_scene() = default;

    public: // Interface
        virtual void resize(VkExtent2D extent) = 0;

        virtual void update(camera const& camera, float delta_time) = 0;

        virtual void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) = 0;

        virtual void draw_imgui() = 0;
    };
} // namespace vkrndr

#endif
