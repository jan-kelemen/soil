#include <bullet_debug_renderer.hpp>

#include <bullet_adapter.hpp> // IWYU pragma: keep

#include <LinearMath/btVector3.h>

#include <fmt/base.h>

#include <spdlog/spdlog.h>

void soil::bullet_debug_renderer::drawLine(btVector3 const& from,
    btVector3 const& to,
    btVector3 const& color)
{
    spdlog::info("Line from {} to {} with color {}", from, to, color);
}

void soil::bullet_debug_renderer::drawContactPoint(btVector3 const& PointOnB,
    btVector3 const& normalOnB,
    btScalar const distance,
    [[maybe_unused]] int const lifeTime,
    btVector3 const& color)
{
    drawLine(PointOnB, PointOnB + normalOnB * distance, color);
    btVector3 const ncolor{0, 0, 0};
    drawLine(PointOnB, PointOnB + normalOnB * 0.01f, ncolor);
}

void soil::bullet_debug_renderer::reportErrorWarning(char const* warningString)
{
    spdlog::error("{}", warningString);
}

void soil::bullet_debug_renderer::draw3dText(
    [[maybe_unused]] btVector3 const& location,
    [[maybe_unused]] char const* textString)
{
}

void soil::bullet_debug_renderer::setDebugMode(int debugMode)
{
    debug_mode_ = debugMode;
}

int soil::bullet_debug_renderer::getDebugMode() const { return debug_mode_; }
