#include <bullet_adapter.hpp>

fmt::format_context::iterator fmt::formatter<btVector3>::format(
    btVector3 const& vec,
    format_context& ctx) const
{
    return format_to(ctx.out(), "({}, {}, {})", vec.x(), vec.y(), vec.z());
}
