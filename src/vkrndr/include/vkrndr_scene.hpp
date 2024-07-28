#ifndef VKRNDR_SCENE_INCLUDED
#define VKRNDR_SCENE_INCLUDED

#include <vkrndr_camera.hpp>

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] scene
    {
    public: // Destruction
        virtual ~scene() = default;

    public: // Interface
        virtual void resize(VkExtent2D extent) = 0;

        virtual void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) = 0;

        virtual void draw_imgui() = 0;
    };
} // namespace vkrndr

#endif
