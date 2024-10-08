#include <application.hpp>

#include <bullet_debug_renderer.hpp>
#include <free_camera_controller.hpp>
#include <heightmap.hpp>
#include <mouse_controller.hpp>
#include <perspective_camera.hpp>
#include <physics_engine.hpp>
#include <terrain.hpp>

#include <cppext_numeric.hpp>

#include <niku_application.hpp>

#include <vkrndr_scene.hpp> // IWYU pragma: keep
#include <vulkan_depth_buffer.hpp>
#include <vulkan_device.hpp>
#include <vulkan_image.hpp>
#include <vulkan_renderer.hpp>

#include <SDL_events.h>
#include <SDL_video.h>

#include <memory>

// IWYU pragma: no_include <BulletCollision/CollisionShapes/btConcaveShape.h>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>

soil::application::application(bool const debug)
    : niku::application(niku::startup_params{
          .init_subsystems = {.video = true, .audio = false, .debug = debug},
          .title = "soil",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512})
    , mouse_{!debug}
    , camera_controller_{&camera_, &mouse_}
    , mouse_controller_{&mouse_, &camera_, &physics_}
{
    vulkan_renderer()->imgui_layer(true);

    fixed_update_interval(1.0f / 60.0f);

    camera_.resize({512, 512});
    camera_.set_position({-512.0f, 200.0f, 512.0f});
    camera_.set_near_far({0.1f, 10000.0f});
    camera_.update();

    resize({512, 512});
}

soil::application::~application() = default;

bool soil::application::handle_event(SDL_Event const& event)
{
    camera_controller_.handle_event(event);
    mouse_controller_.handle_event(event);

    return true;
}

void soil::application::fixed_update(float const delta_time)
{
    physics_.fixed_update(delta_time);
}

void soil::application::update(float const delta_time)
{
    camera_controller_.update(delta_time);

    terrain_->update(camera_, delta_time);

    physics_.update(camera_.position());
    physics_.debug_renderer()->update(camera_, delta_time);
}

vkrndr::scene* soil::application::render_scene() { return this; }

void soil::application::on_startup()
{
    physics_.set_gravity({0.0f, -9.81f, 0.0f});
    // Add heightfield
    {
        heightmap_ = std::make_unique<heightmap>("heightmap.png");

        terrain_ = std::make_unique<terrain>(*heightmap_,
            &physics_,
            this->vulkan_device(),
            this->vulkan_renderer(),
            &color_image_,
            &depth_buffer_);
    }

    physics_.attach_renderer(this->vulkan_device(),
        this->vulkan_renderer(),
        &color_image_,
        &depth_buffer_);
}

void soil::application::on_shutdown()
{
    terrain_.reset();

    physics_.detach_renderer(this->vulkan_device(), this->vulkan_renderer());

    destroy(this->vulkan_device(), &depth_buffer_);
    destroy(this->vulkan_device(), &color_image_);
}

void soil::application::resize(VkExtent2D extent)
{
    destroy(this->vulkan_device(), &color_image_);
    color_image_ = vkrndr::create_image_and_view(this->vulkan_device(),
        extent,
        1,
        this->vulkan_device()->max_msaa_samples,
        this->vulkan_renderer()->image_format(),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    destroy(this->vulkan_device(), &depth_buffer_);
    depth_buffer_ =
        vkrndr::create_depth_buffer(this->vulkan_device(), extent, false);
}

void soil::application::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkExtent2D extent)
{
    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(extent.width),
        .height = cppext::as_fp(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    terrain_->draw(target_image, command_buffer, scissor);
    physics_.debug_renderer()->draw(target_image, command_buffer, scissor);
}

void soil::application::draw_imgui()
{
    terrain_->draw_imgui();
    physics_.debug_renderer()->draw_imgui();
}
