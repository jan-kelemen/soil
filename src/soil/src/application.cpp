#include <application.hpp>

#include <free_camera_controller.hpp>
#include <mouse_controller.hpp>
#include <perspective_camera.hpp>
#include <physics_engine.hpp>

#include <cppext_numeric.hpp>

#include <niku_application.hpp>

#include <vulkan_renderer.hpp>
#include <vulkan_scene.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>

#include <SDL_events.h>
#include <SDL_video.h>

#include <cstddef>
#include <memory>
#include <utility>

// IWYU pragma: no_include <BulletCollision/CollisionShapes/btConcaveShape.h>
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
    , mouse_{!debug}
    , camera_controller_{&camera_, &mouse_}
    , mouse_controller_{&mouse_, &camera_, &physics_}
{
    fixed_update_interval(1.0f / 60.0f);

    camera_.resize({512, 512});
    camera_.set_position({-15.0f, 15.0f, -15.0f});
    camera_.update();
}

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
        transform.setOrigin({0.0f, 10.0f, 0.0f});

        auto* const cube{physics_.add_rigid_body(
            std::make_unique<btBoxShape>(btVector3{0.5f, 0.5f, 0.5f}),
            0.1f,
            transform)};
        cube->setUserIndex(1);
    }

    // Add heightfield
    {
        heightfield_data_.resize(25);

        for (size_t i{}; i != heightfield_data_.size(); ++i)
        {
            heightfield_data_[i] = cppext::as_fp(i % 3);
        }

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin({0.0f, 0.0f, 0.0f});

        auto heightfield_shape{std::make_unique<btHeightfieldTerrainShape>(5,
            5,
            heightfield_data_.data(),
            10.0f,
            0.0f,
            5.0f,
            1,
            PHY_FLOAT,
            false)};

        auto* const heightfield{
            physics_.add_rigid_body(std::move(heightfield_shape),
                0.0f,
                transform)};
        heightfield->setUserIndex(2);
    }

    physics_.attach_renderer(this->vulkan_device(), this->vulkan_renderer());
}

void soil::application::on_shutdown()
{
    physics_.detach_renderer(this->vulkan_device(), this->vulkan_renderer());
}
