#include <mouse_controller.hpp>

#include <perspective_camera.hpp>
#include <physics_engine.hpp>

#include <cppext_numeric.hpp>

#include <niku_mouse.hpp>

#include <BulletDynamics/Dynamics/btRigidBody.h>

#include <glm/vec2.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

#include <spdlog/spdlog.h>

// IWYU pragma: no_include <fmt/base.h>

soil::mouse_controller::mouse_controller(niku::mouse* const mouse,
    perspective_camera* const camera,
    physics_engine* const physics_engine)
    : mouse_{mouse}
    , camera_{camera}
    , physics_engine_{physics_engine}
{
}

void soil::mouse_controller::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN)
    {
        auto const raycast{raycast_to_world()};
        spdlog::info("near ({}, {}, {}), far ({}, {}, {})",
            raycast.first.x,
            raycast.first.y,
            raycast.first.z,
            raycast.second.x,
            raycast.second.y,
            raycast.second.z);
        auto const& [body, point] =
            physics_engine_->raycast(raycast.first, raycast.second);
        if (body)
        {
            spdlog::info("hit {} on ({}, {}, {})",
                body->getUserIndex(),
                point.x,
                point.y,
                point.z);
        }
    }
    else if (event.type == SDL_KEYDOWN)
    {
        auto const& keyboard{event.key};
        if (keyboard.keysym.scancode == SDL_SCANCODE_F3)
        {
            mouse_->set_capture(!mouse_->captured());
        }
    }
}

std::pair<glm::vec3, glm::vec3> soil::mouse_controller::raycast_to_world() const
{
    glm::vec2 const position = [this]()
    {
        if (mouse_->captured())
        {
            auto const extent{camera_->extent()};
            return glm::vec2{cppext::as_fp(extent.x), cppext::as_fp(extent.y)};
        }

        auto const absolute{mouse_->position()};
        return glm::vec2{cppext::as_fp(absolute.x), cppext::as_fp(absolute.y)};
    }();

    return camera_->raycast(position);
}
