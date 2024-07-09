#ifndef BULLET_DEBUG_RENDERER_INCLUDED
#define BULLET_DEBUG_RENDERER_INCLUDED

#include <LinearMath/btIDebugDraw.h>
#include <LinearMath/btScalar.h>

class btVector3;

namespace soil
{
    class [[nodiscard]] bullet_debug_renderer final : public btIDebugDraw
    {
    public:
        bullet_debug_renderer() = default;

        bullet_debug_renderer(bullet_debug_renderer const&) = delete;

        bullet_debug_renderer(bullet_debug_renderer&&) noexcept = delete;

    public:
        ~bullet_debug_renderer() override = default;

    public: // btIDebugDraw overrides
        void drawLine(btVector3 const& from,
            btVector3 const& to,
            btVector3 const& color) override;

        void drawContactPoint(btVector3 const& PointOnB,
            btVector3 const& normalOnB,
            btScalar distance,
            int lifeTime,
            btVector3 const& color) override;

        void reportErrorWarning(char const* warningString) override;

        void draw3dText(btVector3 const& location,
            char const* textString) override;

        void setDebugMode(int debugMode) override;

        [[nodiscard]] int getDebugMode() const override;

    public:
        bullet_debug_renderer& operator=(bullet_debug_renderer const&) = delete;

        bullet_debug_renderer& operator=(bullet_debug_renderer&&) = delete;

    private:
        int debug_mode_{
            btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb};
    };
} // namespace soil

#endif
