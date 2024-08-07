#include <terrain.hpp>

#include <heightmap.hpp>
#include <physics_engine.hpp>
#include <terrain_renderer.hpp>

#include <cppext_numeric.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_renderer.hpp>

#include <BulletCollision/CollisionShapes/btCollisionShape.h> // IWYU pragma: keep
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace
{
    struct [[nodiscard]] chunk_component final
    {
        uint32_t chunk_index;
        glm::vec3 chunk_offset;
    };

    struct [[nodiscard]] physics_component final
    {
        std::vector<float> heights;
        btRigidBody* rigid_body{nullptr};
    };

    [[nodiscard]] std::pair<size_t, size_t> global_position(size_t const x,
        size_t const y,
        size_t const chunk,
        size_t const chunk_dimension,
        size_t const chunks_per_dimension)
    {
        auto const chunk_y{chunk / chunks_per_dimension};
        auto const chunk_x{chunk % chunks_per_dimension};

        return {chunk_x * (chunk_dimension - 1) + x,
            chunk_y * (chunk_dimension - 1) + y};
    }

    void generate_chunks(entt::registry& registry,
        soil::physics_engine& physics_engine,
        soil::heightmap const& heightmap,
        size_t terrain_dimension,
        size_t const chunk_dimension)
    {
        auto const chunks_per_dimension{
            (terrain_dimension - 1) / (chunk_dimension - 1) + 1};
        auto const center_distance{cppext::as_fp(chunk_dimension - 1)};
        auto const center_offset{center_distance / 2.0f};

        auto generate_chunk =
            [&](size_t const chunk_x, size_t const chunk_y) mutable
        {
            auto const id{registry.create()};

            auto const offset_x{cppext::as_fp(chunk_x) * center_distance};
            auto const offset_y{cppext::as_fp(chunk_y) * center_distance};
            auto const& chunk{registry.emplace<chunk_component>(id,
                cppext::narrow<uint32_t>(
                    chunk_y * chunks_per_dimension + chunk_x),
                glm::vec3{-center_offset + offset_x,
                    -127.5f,
                    -center_offset + offset_y})};

            // Skip adding physics to chunks which are not on diagonal
            if (chunk_x != chunk_y)
            {
                return;
            }

            std::vector<float> heights;
            heights.reserve(chunk_dimension * chunk_dimension);
            for (size_t j{}; j != chunk_dimension; ++j)
            {
                for (size_t i{}; i != chunk_dimension; ++i)
                {
                    auto const& [x, y] = global_position(i,
                        j,
                        chunk.chunk_index,
                        chunk_dimension,
                        chunks_per_dimension);
                    heights.push_back(heightmap.value(x, y));
                }
            }

            auto& component{registry.emplace<physics_component>(id,
                std::move(heights),
                nullptr)};

            auto heightfield_shape{std::make_unique<btHeightfieldTerrainShape>(
                cppext::narrow<int>(chunk_dimension),
                cppext::narrow<int>(chunk_dimension),
                component.heights.data(),
                0.0f,
                cppext::as_fp(std::numeric_limits<uint8_t>::max()),
                1,
                false)};

            btTransform transform;
            transform.setIdentity();
            transform.setOrigin({offset_x, 0.0f, offset_y});
            component.rigid_body =
                physics_engine.add_rigid_body(std::move(heightfield_shape),
                    0.0f,
                    transform);
            component.rigid_body->setUserIndex(
                cppext::narrow<int>(chunk.chunk_index));
        };

        for (auto y : std::views::iota(size_t{0}, chunks_per_dimension - 1))
        {
            for (auto x : std::views::iota(size_t{0}, chunks_per_dimension - 1))
            {
                generate_chunk(x, y);
            }
        }
    }
} // namespace

soil::terrain::terrain(heightmap const& heightmap,
    physics_engine* const physics_engine,
    vkrndr::vulkan_device* device,
    vkrndr::vulkan_renderer* renderer,
    vkrndr::vulkan_image* color_image,
    vkrndr::vulkan_image* depth_buffer)
    : physics_engine_{physics_engine}
    , device_{device}
    , terrain_dimension_{cppext::narrow<uint32_t>(heightmap.dimension())}
    , chunk_dimension_{65}
    , heightmap_buffer_{create_buffer(device,
          heightmap.dimension() * heightmap.dimension() * sizeof(float),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , renderer_{device,
          renderer,
          color_image,
          depth_buffer,
          &heightmap_buffer_,
          terrain_dimension_,
          chunk_dimension_}

{
    fill_heightmap(heightmap, renderer);

    generate_chunks(chunk_registry_,
        *physics_engine_,
        heightmap,
        terrain_dimension_,
        chunk_dimension_);
}

soil::terrain::~terrain() { destroy(device_, &heightmap_buffer_); }

void soil::terrain::update(soil::perspective_camera const& camera,
    [[maybe_unused]] float delta_time)
{
    renderer_.update(camera);
}

void soil::terrain::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkRect2D render_area)
{
    auto const guard{
        renderer_.begin_render_pass(target_image, command_buffer, render_area)};

    for (auto entity : chunk_registry_.view<chunk_component>())
    {
        auto const& chunk_comp{chunk_registry_.get<chunk_component>(entity)};
        renderer_.draw(command_buffer,
            static_cast<uint32_t>(lod_),
            chunk_comp.chunk_index,
            glm::translate(glm::mat4{1.0f}, chunk_comp.chunk_offset));
    }

    renderer_.end_render_pass();
}

void soil::terrain::draw_imgui()
{
    ImGui::Begin("Terrain");
    ImGui::SliderInt("LOD", &lod_, 0, renderer_.lod_levels());
    ImGui::End();

    renderer_.draw_imgui();
}

void soil::terrain::fill_heightmap(heightmap const& heightmap,
    vkrndr::vulkan_renderer* const renderer)
{
    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        heightmap_buffer_.size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto values{heightmap.data()};
    assert(values.size_bytes() == heightmap_buffer_.size);
    memcpy(staging_map.as<void>(), values.data(), values.size_bytes());
    unmap_memory(device_, &staging_map);

    renderer->transfer_buffer(staging_buffer, heightmap_buffer_);

    destroy(device_, &staging_buffer);
}
