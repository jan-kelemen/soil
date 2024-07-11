#ifndef SOIL_PERSPECTIVE_CAMERA_INCLUDED
#define SOIL_PERSPECTIVE_CAMERA_INCLUDED

#include <niku_perspective_camera.hpp>

#include <glm/vec2.hpp>

namespace soil
{
    class [[nodiscard]] perspective_camera : public niku::perspective_camera
    {
    public:
        perspective_camera() = default;

        perspective_camera(perspective_camera const&) = default;

        perspective_camera(perspective_camera&&) noexcept = default;

    public:
        ~perspective_camera() override = default;

    public:
        void resize(glm::uvec2 const& extent);

        void set_position(glm::vec3 const& position);

    public:
        perspective_camera& operator=(perspective_camera const&) = default;

        perspective_camera& operator=(perspective_camera&&) noexcept = default;

    private:
        glm::uvec2 extent_;
    };
} // namespace soil
#endif // !SOIL_PERSPECTIVE_CAMERA_INCLUDED
