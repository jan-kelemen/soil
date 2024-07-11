#include <perspective_camera.hpp>

#include <cppext_numeric.hpp>

void soil::perspective_camera::resize(glm::uvec2 const& extent)
{
    extent_ = extent;
    aspect_ratio_ = cppext::as_fp(extent.x) / cppext::as_fp(extent.y);
}

void soil::perspective_camera::set_position(glm::vec3 const& position)
{
    position_ = position;
}
