#include <free_camera_controller.hpp>

#include <perspective_camera.hpp>

#include <cppext_numeric.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>

#include <cstdint>

namespace
{
    constexpr auto velocity_factor{3.0f};
} // namespace

soil::free_camera_controller::free_camera_controller(perspective_camera* camera)
    : camera_{camera}
    , velocity_{0.0f, 0.0f, 0.0f}
{
}

void soil::free_camera_controller::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_WINDOWEVENT)
    {
        auto const& window{event.window};
        if (window.event == SDL_WINDOWEVENT_RESIZED ||
            window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            camera_->resize({cppext::narrow<uint32_t>(window.data1),
                cppext::narrow<uint32_t>(window.data2)});
            update_needed_ = true;
        }
    }
    if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN)
    {
        int keyboard_state_length; // NOLINT
        uint8_t const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        velocity_ = {0.0f, 0.0f, 0.0f};

        if (keyboard_state[SDL_SCANCODE_LEFT] != 0)
        {
            velocity_ -= velocity_factor * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT] != 0)
        {
            velocity_ += velocity_factor * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_UP] != 0)
        {
            velocity_ += velocity_factor * camera_->front_direction();
        }

        if (keyboard_state[SDL_SCANCODE_DOWN] != 0)
        {
            velocity_ -= velocity_factor * camera_->front_direction();
        }

        update_needed_ = true;
    }
}

void soil::free_camera_controller::update(float delta_time)
{
    if (update_needed_)
    {
        camera_->set_position(camera_->position() + velocity_ * delta_time);

        camera_->update();
    }
}
