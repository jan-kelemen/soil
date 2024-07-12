#ifndef SOIL_APPLICATION_INCLUDED
#define SOIL_APPLICATION_INCLUDED

#include <perspective_camera.hpp>
#include <physics_engine.hpp>

#include <niku_application.hpp>

#include <SDL2/SDL_events.h>

#include <vector>

namespace vkrndr
{
    class vulkan_renderer;
} // namespace vkrndr

namespace soil
{
    class [[nodiscard]] application final : public niku::application
    {
    public:
        explicit application(bool debug);

        application(application const&) = delete;

        application(application&&) noexcept = delete;

    public:
        ~application() override = default;

    public:
        // cppcheck-suppress duplInheritedMember
        application& operator=(application const&) = delete;

        // cppcheck-suppress duplInheritedMember
        application& operator=(application&&) noexcept = delete;

    private: // niku::application callback interface
        bool handle_event([[maybe_unused]] SDL_Event const& event) override;

        void fixed_update(float delta_time) override;

        void update(float delta_time) override;

        void render(vkrndr::vulkan_renderer* renderer) override;

        void on_startup() override;

        void on_shutdown() override;

    private:
        physics_engine physics_;
        perspective_camera camera_;

        std::vector<float> heightfield_data_;
    };
} // namespace soil

#endif
