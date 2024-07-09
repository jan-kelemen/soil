#ifndef BULLET_ADAPTER_INCLUDED
#define BULLET_ADAPTER_INCLUDED

#include <LinearMath/btVector3.h>

#include <fmt/base.h>

#include <glm/vec3.hpp>

namespace soil
{
    [[nodiscard]] inline btVector3 to_bullet(glm::vec3 const& vec)
    {
        return {vec.x, vec.y, vec.z};
    }
} // namespace soil

template<>
struct fmt::formatter<btVector3>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    format_context::iterator format(btVector3 const& c,
        format_context& ctx) const;
};

#endif
