#include <terrain.hpp>

#include <heightmap.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_renderer.hpp>

#include <imgui.h>

#include <array>

soil::terrain::terrain(heightmap const& heightmap,
    vkrndr::vulkan_device* device,
    vkrndr::vulkan_renderer* renderer,
    vkrndr::vulkan_image* color_image,
    vkrndr::vulkan_image* depth_buffer)
    : device_{device}
    , terrain_dimension_{cppext::narrow<uint32_t>(heightmap.dimension())}
    , chunk_dimension_{65}
    , vertex_count_{chunk_dimension_ * chunk_dimension_}
    , vertex_buffer_{create_buffer(device,
          vertex_count_ * sizeof(terrain_vertex),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
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
    fill_vertex_buffer(renderer);
}

soil::terrain::~terrain()
{
    destroy(device_, &heightmap_buffer_);
    destroy(device_, &vertex_buffer_);
}

void soil::terrain::update(soil::perspective_camera const& camera,
    [[maybe_unused]] float delta_time)
{
    renderer_.update(camera);
}

void soil::terrain::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkRect2D render_area)
{
    float const center_distance{cppext::as_fp(chunk_dimension_ - 1)};
    float const center_offset{cppext::as_fp(terrain_dimension_ - 1) / 2.0f};

    // Heightmap values range from [0, 255], bullet recenters it to [-127.5,
    // 127.5]
    glm::vec3 offset{center_offset, 127.5f, 49.5f}; // t00ning

    glm::mat4 model_matrix{1.0f};

    auto const guard{
        renderer_.begin_render_pass(target_image, command_buffer, render_area)};

    glm::mat4 const center_model{glm::translate(model_matrix, -offset)};
    for (uint32_t j{}; j != 16; ++j)
    {
        for (uint32_t i{}; i != 16; ++i)
        {
            renderer_.draw(command_buffer,
                static_cast<uint32_t>(lod_),
                j * 17 + i,
                &vertex_buffer_,
                glm::translate(center_model,
                    {cppext::as_fp(i) * center_distance,
                        0.0f,
                        cppext::as_fp(j) * center_distance}));
        }
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

void soil::terrain::fill_vertex_buffer(vkrndr::vulkan_renderer* const renderer)
{
    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        vertex_count_ * sizeof(terrain_vertex),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* const vertices{staging_map.as<terrain_vertex>()};
    for (uint32_t z{}; z != chunk_dimension_; ++z)
    {
        for (uint32_t x{}; x != chunk_dimension_; ++x)
        {
            vertices[z * chunk_dimension_ + x] = {.position = {x, z}};
        }
    }

    renderer->transfer_buffer(staging_buffer, vertex_buffer_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}
