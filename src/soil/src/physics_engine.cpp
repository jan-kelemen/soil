#include <physics_engine.hpp>

#include <bullet_adapter.hpp>
#include <bullet_debug_renderer.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btMotionState.h>
#include <LinearMath/btVector3.h>

#include <glm/vec3.hpp>

#include <memory>
#include <utility>

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

    void update();

    [[nodiscard]] vkrndr::vulkan_scene* render_scene();

    void attach_renderer(vkrndr::vulkan_device* device,
        vkrndr::vulkan_renderer* renderer);

    void detach_renderer(vkrndr::vulkan_device* device,
        vkrndr::vulkan_renderer* renderer);

public:
    void set_gravity(glm::vec3 const& gravity);

    btRigidBody* add_rigid_body(std::unique_ptr<btCollisionShape> shape,
        float mass,
        btTransform const& transform);

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

private:
    btDefaultCollisionConfiguration collision_configuration_;
    btCollisionDispatcher dispatcher_;
    btDbvtBroadphase overlapping_pair_cache_;
    btSequentialImpulseConstraintSolver solver_;
    btDiscreteDynamicsWorld world_;

    btAlignedObjectArray<btCollisionShape*> collision_shapes_;

    std::unique_ptr<bullet_debug_renderer> debug_renderer_;
};

soil::physics_engine::impl::impl()
    : dispatcher_{&collision_configuration_}
    , world_{&dispatcher_,
          &overlapping_pair_cache_,
          &solver_,
          &collision_configuration_}
{
}

soil::physics_engine::impl::~impl()
{
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    // remove the rigidbodies from the dynamics world and delete them
    for (int i{world_.getNumCollisionObjects() - 1}; i >= 0; --i)
    {
        btCollisionObject* obj{world_.getCollisionObjectArray()[i]};

        if (btRigidBody* const body{btRigidBody::upcast(obj)};
            body && body->getMotionState())
        {
            delete body->getMotionState();
        }

        world_.removeCollisionObject(obj);
        delete obj;
    }

    // delete collision shapes
    for (int j{}; j < collision_shapes_.size(); ++j)
    {
        delete std::exchange(collision_shapes_[j], nullptr);
    }
    collision_shapes_.clear();
    // NOLINTEND(cppcoreguidelines-owning-memory)
}

void soil::physics_engine::impl::fixed_update(float const delta_time)
{
    world_.stepSimulation(delta_time, 10);
}

void soil::physics_engine::impl::set_gravity(glm::vec3 const& gravity)
{
    world_.setGravity(to_bullet(gravity));
}

void soil::physics_engine::impl::update() { world_.debugDrawWorld(); }

vkrndr::vulkan_scene* soil::physics_engine::impl::render_scene()
{
    return debug_renderer_.get();
}

void soil::physics_engine::impl::attach_renderer(
    vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer)
{
    debug_renderer_ = std::make_unique<bullet_debug_renderer>(device, renderer);
    world_.setDebugDrawer(debug_renderer_.get());
}

void soil::physics_engine::impl::detach_renderer(
    [[maybe_unused]] vkrndr::vulkan_device* const device,
    [[maybe_unused]] vkrndr::vulkan_renderer* const renderer)
{
    world_.setDebugDrawer(nullptr);
    debug_renderer_ = nullptr;
}

btRigidBody* soil::physics_engine::impl::add_rigid_body(
    std::unique_ptr<btCollisionShape> shape,
    float const mass,
    btTransform const& transform)
{
    btVector3 local_inertia{0, 0, 0};
    if (shape && mass != 0.0f)
    {
        shape->calculateLocalInertia(mass, local_inertia);
    }
    collision_shapes_.push_back(shape.release());

    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    btDefaultMotionState* const motion_state{
        new btDefaultMotionState{transform}};
    btRigidBody::btRigidBodyConstructionInfo const rigid_body_info{mass,
        motion_state,
        collision_shapes_[collision_shapes_.size() - 1],
        local_inertia};
    btRigidBody* const body{new btRigidBody{rigid_body_info}};
    // NOLINTEND(cppcoreguidelines-owning-memory)

    // add the body to the dynamics world
    world_.addRigidBody(body);

    return body;
}

soil::physics_engine::physics_engine() : impl_{std::make_unique<impl>()} { }

soil::physics_engine::~physics_engine() = default;

void soil::physics_engine::fixed_update(float const delta_time)
{
    impl_->fixed_update(delta_time);
}

void soil::physics_engine::update() { impl_->update(); }

vkrndr::vulkan_scene* soil::physics_engine::render_scene()
{
    return impl_->render_scene();
}

void soil::physics_engine::attach_renderer(vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer)
{
    impl_->attach_renderer(device, renderer);
}

void soil::physics_engine::detach_renderer(vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer)
{
    impl_->detach_renderer(device, renderer);
}

void soil::physics_engine::set_gravity(glm::vec3 const& gravity)
{
    impl_->set_gravity(gravity);
}

btRigidBody* soil::physics_engine::add_rigid_body(
    std::unique_ptr<btCollisionShape> shape,
    float const mass,
    btTransform const& transform)
{
    return impl_->add_rigid_body(std::move(shape), mass, transform);
}
