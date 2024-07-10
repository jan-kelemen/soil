#ifndef SOIL_APPLICATION_INCLUDED
#define SOIL_APPLICATION_INCLUDED

#include <physics_engine.hpp>

#include <niku_application.hpp>

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
        void fixed_update(float delta_time) override;

        void update(float delta_time) override;

        void render(vkrndr::vulkan_renderer* renderer) override;

        void on_startup() override;

        void on_shutdown() override;

    private:
        physics_engine physics_;
    };
} // namespace soil

#endif
