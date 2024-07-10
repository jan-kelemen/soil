#include <application.hpp>

#include <physics_engine.hpp>

#include <niku_application.hpp>

#include <vulkan_renderer.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>

#include <SDL_video.h>
#include <memory>

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
}

void soil::application::fixed_update(float const delta_time)
{
    physics_.fixed_update(delta_time);
}

void soil::application::update([[maybe_unused]] float const delta_time)
{
    physics_.update();
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
