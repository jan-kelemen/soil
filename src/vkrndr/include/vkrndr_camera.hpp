#ifndef VKRNDR_CAMERA_INCLUDED
#define VKRNDR_CAMERA_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] camera
    {
    public:
        virtual ~camera() = default;

    public:
        virtual void set_aspect_ratio(float aspect_ratio) = 0;

        [[nodiscard]] virtual float aspect_ratio() const = 0;

        virtual void set_position(glm::vec3 const& position) = 0;

        [[nodiscard]] virtual glm::vec3 const& position() const = 0;

        [[nodiscard]] virtual glm::mat4 const& view_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const& projection_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const&
        view_projection_matrix() const = 0;
    };
} // namespace vkrndr
#endif
