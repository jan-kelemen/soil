#ifndef SOIL_PHYSICS_ENGINE_INCLUDED
#define SOIL_PHYSICS_ENGINE_INCLUDED

#include <glm/vec3.hpp>

#include <memory>

class btCollisionShape;
class btTransform;
class btRigidBody;

namespace soil
{
    class [[nodiscard]] physics_engine final
    {
    public:
        physics_engine();

        physics_engine(physics_engine const&) = delete;

        physics_engine(physics_engine&&) noexcept = default;

    public:
        ~physics_engine();

    public:
        void fixed_update(float delta_time);

        void draw();

    public:
        void set_gravity(glm::vec3 const& gravity);

        btRigidBody* add_rigid_body(std::unique_ptr<btCollisionShape> shape,
            float mass,
            btTransform const& transform);

    public:
        physics_engine& operator=(physics_engine const&) = delete;

        physics_engine& operator=(physics_engine&&) noexcept = delete;

    private:
        class impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace soil

#endif
