#include <physics_engine.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <LinearMath/btVector3.h>

#include <glm/vec3.hpp>

namespace
{
    [[nodiscard]] btVector3 to_bullet(glm::vec3 const& vec)
    {
        return {vec.x, vec.y, vec.z};
    }
} // namespace

class [[nodiscard]] soil::physics_engine::impl final
{
public:
    impl();

    impl(impl const&) = delete;

    impl(impl&&) noexcept = delete;

public:
    ~impl();

public:
    void fixed_update(float delta_time);

    void set_gravity(glm::vec3 const& gravity);

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

private:
    btDefaultCollisionConfiguration collision_configuration_;
    btCollisionDispatcher dispatcher_;
    btDbvtBroadphase overlapping_pair_cache_;
    btSequentialImpulseConstraintSolver solver_;
    btDiscreteDynamicsWorld world_;
};

soil::physics_engine::impl::impl()
    : dispatcher_{&collision_configuration_}
    , world_{&dispatcher_,
          &overlapping_pair_cache_,
          &solver_,
          &collision_configuration_}
{
}

soil::physics_engine::impl::~impl() = default;

void soil::physics_engine::impl::fixed_update(float const delta_time)
{
    world_.stepSimulation(delta_time, 10);
}

void soil::physics_engine::impl::set_gravity(glm::vec3 const& gravity)
{
    world_.setGravity(to_bullet(gravity));
}

soil::physics_engine::physics_engine() : impl_{std::make_unique<impl>()} { }

soil::physics_engine::~physics_engine() = default;

void soil::physics_engine::fixed_update(float const delta_time)
{
    impl_->fixed_update(delta_time);
}

void soil::physics_engine::set_gravity(glm::vec3 const& gravity)
{
    impl_->set_gravity(gravity);
}
