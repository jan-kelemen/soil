#ifndef SOIL_PERSPECTIVE_CAMERA_INCLUDED
#define SOIL_PERSPECTIVE_CAMERA_INCLUDED

#include <niku_perspective_camera.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <utility>

namespace soil
{
    class [[nodiscard]] perspective_camera : public niku::perspective_camera
    {
    public:
        perspective_camera();

        perspective_camera(perspective_camera const&) = default;

        perspective_camera(perspective_camera&&) noexcept = default;

    public:
        ~perspective_camera() override = default;

    public:
        [[nodiscard]] glm::uvec2 const& extent() const;

        void resize(glm::uvec2 const& extent);

        [[nodiscard]] std::pair<glm::vec3, glm::vec3> raycast(
            glm::vec2 const& position) const;

    public:
        // cppcheck-suppress duplInheritedMember
        perspective_camera& operator=(perspective_camera const&) = default;

        // cppcheck-suppress duplInheritedMember
        perspective_camera& operator=(perspective_camera&&) noexcept = default;

    private:
        glm::uvec2 extent_;
    };
} // namespace soil
#endif // !SOIL_PERSPECTIVE_CAMERA_INCLUDED
