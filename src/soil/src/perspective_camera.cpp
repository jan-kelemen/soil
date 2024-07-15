#include <perspective_camera.hpp>

#include <cppext_numeric.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

soil::perspective_camera::perspective_camera() : extent_{1920, 1080} { }

glm::uvec2 const& soil::perspective_camera::extent() const { return extent_; }

void soil::perspective_camera::resize(glm::uvec2 const& extent)
{
    extent_ = extent;
    aspect_ratio_ = cppext::as_fp(extent.x) / cppext::as_fp(extent.y);
}

std::pair<glm::vec3, glm::vec3> soil::perspective_camera::raycast(
    glm::vec2 const& position) const
{
    glm::vec4 const viewport{0, 0, extent_.x, extent_.y};

    auto const near{glm::unProjectZO(glm::vec3{position, 0.0f},
        view_matrix_,
        projection_matrix_,
        viewport)};

    auto const far{glm::unProjectZO(glm::vec3{position, 1},
        view_matrix_,
        projection_matrix_,
        viewport)};

    return std::make_pair(near, far);
}
