#ifndef SOIL_MOUSE_CONTROLLER_INCLUDED
#define SOIL_MOUSE_CONTROLLER_INCLUDED

#include <glm/vec3.hpp>

#include <SDL2/SDL_events.h>

#include <utility>

namespace niku
{
    class mouse;
} // namespace niku

namespace soil
{
    class perspective_camera;
    class physics_engine;
} // namespace soil

namespace soil
{
    class [[nodiscard]] mouse_controller final
    {
    public:
        mouse_controller(niku::mouse* mouse,
            perspective_camera* camera,
            physics_engine* physics_engine);

        mouse_controller(mouse_controller const&) = default;

        mouse_controller(mouse_controller&&) noexcept = default;

    public:
        ~mouse_controller() = default;

    public:
        void handle_event(SDL_Event const& event);

    public:
        mouse_controller& operator=(mouse_controller const&) = default;

        mouse_controller& operator=(mouse_controller&&) noexcept = default;

    private:
        [[nodiscard]] std::pair<glm::vec3, glm::vec3> raycast_to_world() const;

    private:
        niku::mouse* mouse_;
        perspective_camera* camera_;
        physics_engine* physics_engine_;
    };
} // namespace soil

#endif
