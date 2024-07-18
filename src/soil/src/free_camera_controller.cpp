#include <free_camera_controller.hpp>

#include <perspective_camera.hpp>

#include <cppext_numeric.hpp>

#include <niku_mouse.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr auto velocity_factor{10.00f};
} // namespace

soil::free_camera_controller::free_camera_controller(
    perspective_camera* const camera,
    niku::mouse* const mouse)
    : camera_{camera}
    , mouse_{mouse}
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
    else if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN)
    {
        int keyboard_state_length; // NOLINT
        uint8_t const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        velocity_ = {0.0f, 0.0f, 0.0f};

        if (keyboard_state[SDL_SCANCODE_LEFT] != 0 ||
            keyboard_state[SDL_SCANCODE_A] != 0)
        {
            velocity_ -= velocity_factor * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT] != 0 ||
            keyboard_state[SDL_SCANCODE_D] != 0)
        {
            velocity_ += velocity_factor * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_UP] != 0 ||
            keyboard_state[SDL_SCANCODE_W] != 0)
        {
            velocity_ += velocity_factor * camera_->front_direction();
        }

        if (keyboard_state[SDL_SCANCODE_DOWN] != 0 ||
            keyboard_state[SDL_SCANCODE_S] != 0)
        {
            velocity_ -= velocity_factor * camera_->front_direction();
        }

        update_needed_ = true;
    }
    else if (event.type == SDL_MOUSEMOTION && mouse_->captured())
    {
        constexpr auto mouse_sensitivity_{0.1f};

        auto const& yaw_pitch{camera_->yaw_pitch()};
        auto const& mouse_offset{mouse_->relative_offset()};

        auto const yaw{
            yaw_pitch.x + cppext::as_fp(mouse_offset.x) * mouse_sensitivity_};
        auto const pitch{
            yaw_pitch.y + cppext::as_fp(-mouse_offset.y) * mouse_sensitivity_};

        camera_->set_yaw_pitch(
            {fmodf(yaw, 360), std::clamp(pitch, -85.0f, 85.0f)});

        update_needed_ = true;
    }
}

void soil::free_camera_controller::update(float delta_time)
{
    if (update_needed_ || velocity_ != glm::vec3{0.0f, 0.0f, 0.0f})
    {
        camera_->set_position(camera_->position() + velocity_ * delta_time);

        camera_->update();

        update_needed_ = false;
    }
}
