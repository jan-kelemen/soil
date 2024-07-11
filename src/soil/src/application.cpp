#include <application.hpp>

#include <perspective_camera.hpp>
#include <physics_engine.hpp>

#include <cppext_numeric.hpp>

#include <niku_application.hpp>

#include <vulkan_renderer.hpp>
#include <vulkan_scene.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>

#include <SDL_events.h>
#include <SDL_video.h>

#include <cstdint>
#include <memory>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

soil::application::application(bool debug)
    : niku::application(niku::startup_params{
          .init_subsystems = {.video = true, .audio = false, .debug = debug},
          .title = "soil",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512})
{
    fixed_update_interval(1.0f / 60.0f);

    camera_.resize({512, 512});
    camera_.set_position({-5.0f, -5.0f, -5.0f});
}

bool soil::application::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_WINDOWEVENT)
    {
        auto const& window{event.window};
        if (window.event == SDL_WINDOWEVENT_RESIZED ||
            window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            camera_.resize({cppext::narrow<uint32_t>(window.data1),
                cppext::narrow<uint32_t>(window.data2)});
        }
    }

    return true;
}

void soil::application::fixed_update(float const delta_time)
{
    physics_.fixed_update(delta_time);
}

void soil::application::update([[maybe_unused]] float const delta_time)
{
    camera_.update();

    physics_.update();
    physics_.render_scene()->update(camera_, delta_time);
}

void soil::application::render(vkrndr::vulkan_renderer* renderer)
{
    renderer->draw(physics_.render_scene());
}

void soil::application::on_startup()
{
    physics_.set_gravity({0.0f, -9.81f, 0.0f});

    // Add static cube
    {
        btTransform transform;
        transform.setIdentity();

        physics_.add_rigid_body(
            std::make_unique<btBoxShape>(btVector3{0.5f, 0.5f, 0.5f}),
            0.0f,
            transform);
    }

    physics_.attach_renderer(this->vulkan_device(), this->vulkan_renderer());
}

void soil::application::on_shutdown()
{
    physics_.detach_renderer(this->vulkan_device(), this->vulkan_renderer());
}
