#ifndef BULLET_DEBUG_RENDERER_INCLUDED
#define BULLET_DEBUG_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_scene.hpp>

#include <LinearMath/btIDebugDraw.h>
#include <LinearMath/btScalar.h>
#include <LinearMath/btVector3.h>

#include <glm/vec3.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_image;
    struct vulkan_pipeline;
    class vulkan_renderer;
    class camera;
} // namespace vkrndr

namespace soil
{
    class [[nodiscard]] bullet_debug_renderer final
        : public btIDebugDraw
        , public vkrndr::vulkan_scene
    {
    public:
        bullet_debug_renderer(vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer,
            vkrndr::vulkan_image* color_image,
            vkrndr::vulkan_image* depth_buffer);

        bullet_debug_renderer(bullet_debug_renderer const&) = delete;

        bullet_debug_renderer(bullet_debug_renderer&&) noexcept = delete;

    public:
        ~bullet_debug_renderer() override;

    public:
        void set_camera_position(glm::vec3 const& position);

    public: // btIDebugDraw overrides
        void drawLine(btVector3 const& from,
            btVector3 const& to,
            btVector3 const& color) override;

        void drawContactPoint(btVector3 const& PointOnB,
            btVector3 const& normalOnB,
            btScalar distance,
            int lifeTime,
            btVector3 const& color) override;

        void reportErrorWarning(char const* warningString) override;

        void draw3dText(btVector3 const& location,
            char const* textString) override;

        void setDebugMode(int debugMode) override;

        [[nodiscard]] int getDebugMode() const override;

    public: // vulkan_scene overrides
        void resize(VkExtent2D extent) override;

        void update(vkrndr::camera const& camera, float delta_time) override;

        void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) override;

        void draw_imgui() override;

    public:
        bullet_debug_renderer& operator=(bullet_debug_renderer const&) = delete;

        bullet_debug_renderer& operator=(bullet_debug_renderer&&) = delete;

    private:
        struct [[nodiscard]] frame_resources final
        {
            vkrndr::vulkan_buffer vertex_buffer;
            vkrndr::mapped_memory vertex_map{};
            uint32_t vertex_count{};

            vkrndr::vulkan_buffer vertex_uniform;
            vkrndr::mapped_memory uniform_map{};
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private: // Physics data
        int debug_mode_{
            btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb};

    private: // Render data
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;
        vkrndr::vulkan_image* color_image_;
        vkrndr::vulkan_image* depth_buffer_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        std::unique_ptr<vkrndr::vulkan_pipeline> line_pipeline_;

        btVector3 camera_position_;
        bool cull_geometry_{true};
        float cull_distance_{30.0f};

        cppext::cycled_buffer<frame_resources> frame_data_;
    };
} // namespace soil

#endif
