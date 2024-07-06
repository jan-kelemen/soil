#include <cppext_numeric.hpp>

#include <sdl_window.hpp>
#include <vulkan_context.hpp>
#include <vulkan_device.hpp>
#include <vulkan_renderer.hpp>

#include <imgui_impl_sdl2.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstdlib>

namespace
{
#ifdef NDEBUG
    constexpr bool enable_validation_layers{false};
#else
    constexpr bool enable_validation_layers{true};
#endif

    [[nodiscard]] bool is_quit_event(SDL_Event const& event,
        SDL_Window* const window)
    {
        switch (event.type)
        {
        case SDL_QUIT:
            return true;
        case SDL_WINDOWEVENT:
        {
            SDL_WindowEvent const& window_event{event.window};
            return window_event.event == SDL_WINDOWEVENT_CLOSE &&
                window_event.windowID == SDL_GetWindowID(window);
        }
        default:
            return false;
        }
    }
} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    vkrndr::sdl_guard const sdl_guard{SDL_INIT_VIDEO};

    vkrndr::sdl_window window{"soil",
        static_cast<SDL_WindowFlags>(
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
        true,
        512,
        512};

    auto context{vkrndr::create_context(&window, enable_validation_layers)};
    auto device{vkrndr::create_device(context)};
    {
        vkrndr::vulkan_renderer renderer{&window, &context, &device};
        renderer.set_imgui_layer(enable_validation_layers);

        uint64_t last_tick{SDL_GetPerformanceCounter()};
        uint64_t last_fixed_tick{last_tick};

        bool done{false};
        while (!done)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0)
            {
                if (enable_validation_layers)
                {
                    ImGui_ImplSDL2_ProcessEvent(&event);
                }

                if (is_quit_event(event, window.native_handle()))
                {
                    done = true;
                }
            }

            uint64_t const current_tick{SDL_GetPerformanceCounter()};
            [[maybe_unused]]
            float const delta{cppext::as_fp(current_tick - last_tick) /
                cppext::as_fp(SDL_GetPerformanceFrequency())};
            [[maybe_unused]]
            float const fixed_delta{
                cppext::as_fp(current_tick - last_fixed_tick) /
                cppext::as_fp(SDL_GetPerformanceFrequency())};
            bool const do_fixed_update{fixed_delta >= 1.0f / 60.0f};

            last_tick = current_tick;

            renderer.begin_frame();
            if (do_fixed_update)
            {
                last_fixed_tick = current_tick;
            }
            renderer.end_frame();
        }

        vkDeviceWaitIdle(device.logical);
    }

    destroy(&device);
    destroy(&context);
    return EXIT_SUCCESS;
}
