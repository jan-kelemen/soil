#ifndef BULLET_DEBUG_RENDERER_INCLUDED
#define BULLET_DEBUG_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_scene.hpp>

#include <LinearMath/btIDebugDraw.h>
#include <LinearMath/btScalar.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>

class btVector3;

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_pipeline;
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class [[nodiscard]] bullet_debug_renderer final
        : public btIDebugDraw
        , public vkrndr::vulkan_scene
    {
    public:
        bullet_debug_renderer(vkrndr::vulkan_device* device,
            vkrndr::vulkan_renderer* renderer);

        bullet_debug_renderer(bullet_debug_renderer const&) = delete;

        bullet_debug_renderer(bullet_debug_renderer&&) noexcept = delete;

    public:
        ~bullet_debug_renderer() override;

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
        [[nodiscard]] VkClearValue clear_color() override;

        [[nodiscard]] VkClearValue clear_depth() override;

        [[nodiscard]] vkrndr::vulkan_image* depth_image() override;

        void resize(VkExtent2D extent) override;

        void draw(VkCommandBuffer command_buffer, VkExtent2D extent) override;

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
        };

    private: // Physics data
        int debug_mode_{
            btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb};

    private: // Render data
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;

        cppext::cycled_buffer<frame_resources> frame_data_;
        vkrndr::vulkan_image depth_buffer_;
        std::unique_ptr<vkrndr::vulkan_pipeline> line_pipeline_;
    };
} // namespace soil

#endif
