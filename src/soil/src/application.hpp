#ifndef SOIL_APPLICATION_INCLUDED
#define SOIL_APPLICATION_INCLUDED

#include <free_camera_controller.hpp>
#include <mouse_controller.hpp>
#include <perspective_camera.hpp>
#include <physics_engine.hpp>

#include <niku_application.hpp>
#include <niku_mouse.hpp>

#include <vkrndr_scene.hpp>
#include <vulkan_image.hpp>

#include <SDL2/SDL_events.h>

#include <vulkan/vulkan_core.h>

#include <memory>

namespace soil
{
    class heightmap;
    class terrain_renderer;
} // namespace soil

namespace soil
{
    class [[nodiscard]] application final
        : public niku::application
        , private vkrndr::scene
    {
    public:
        explicit application(bool debug);

        application(application const&) = delete;

        application(application&&) noexcept = delete;

    public:
        ~application() override;

    public:
        // cppcheck-suppress duplInheritedMember
        application& operator=(application const&) = delete;

        // cppcheck-suppress duplInheritedMember
        application& operator=(application&&) noexcept = delete;

    private: // niku::application callback interface
        bool handle_event([[maybe_unused]] SDL_Event const& event) override;

        void fixed_update(float delta_time) override;

        void update(float delta_time) override;

        [[nodiscard]] vkrndr::scene* render_scene() override;

        void on_startup() override;

        void on_shutdown() override;

    public: // vulkan_scene overrides
        void resize(VkExtent2D extent) override;

        void draw(VkImageView target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) override;

        void draw_imgui() override;

    private:
        physics_engine physics_;
        perspective_camera camera_;
        niku::mouse mouse_;

        free_camera_controller camera_controller_;
        mouse_controller mouse_controller_;

        std::unique_ptr<heightmap> heightmap_;
        std::unique_ptr<terrain_renderer> terrain_renderer_;

        vkrndr::vulkan_image color_image_;
        vkrndr::vulkan_image depth_buffer_;
    };
} // namespace soil

#endif
