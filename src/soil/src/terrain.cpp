#include <terrain.hpp>

#include <heightmap.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_renderer.hpp>

#include <imgui.h>

soil::terrain::terrain(vkrndr::vulkan_device* device,
    vkrndr::vulkan_renderer* renderer,
    vkrndr::vulkan_image* color_image,
    vkrndr::vulkan_image* depth_buffer,
    heightmap const& heightmap)
    : device_{device}
    , vertex_count_{cppext::narrow<uint32_t>(
          heightmap.dimension() * heightmap.dimension())}
    , vertex_buffer_{create_buffer(device,
          vertex_count_ * sizeof(terrain_vertex),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , heightmap_{create_buffer(device,
          vertex_count_ * sizeof(float),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , renderer_{device,
          renderer,
          color_image,
          depth_buffer,
          &heightmap_,
          cppext::narrow<uint32_t>(heightmap.dimension())}
{
    fill_heightmap(renderer, heightmap);
    fill_vertex_buffer(renderer,
        cppext::narrow<uint32_t>(heightmap.dimension()));
}

soil::terrain::~terrain()
{
    destroy(device_, &heightmap_);
    destroy(device_, &vertex_buffer_);
}

void soil::terrain::update(soil::perspective_camera const& camera,
    float delta_time)
{
    renderer_.update(camera, delta_time);
}

void soil::terrain::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkRect2D render_area)
{
    auto guard{
        renderer_.begin_render_pass(target_image, command_buffer, render_area)};

    renderer_.draw(command_buffer,
        static_cast<uint32_t>(lod_),
        vertex_buffer_.buffer,
        0);

    renderer_.end_render_pass();
}

void soil::terrain::draw_imgui()
{
    ImGui::Begin("Terrain");
    ImGui::SliderInt("LOD", &lod_, 0, renderer_.lod_levels());
    ImGui::End();

    renderer_.draw_imgui();
}

void soil::terrain::fill_heightmap(vkrndr::vulkan_renderer* const renderer,
    heightmap const& heightmap)
{
    auto const dimension{heightmap.dimension()};

    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        vertex_count_ * sizeof(float),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* const heights{staging_map.as<float>()};
    for (size_t z{}; z != dimension; ++z)
    {
        for (size_t x{}; x != dimension; ++x)
        {
            heights[z * dimension + x] = heightmap.value(x, z);
        }
    }

    renderer->transfer_buffer(staging_buffer, heightmap_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}

void soil::terrain::fill_vertex_buffer(vkrndr::vulkan_renderer* const renderer,
    uint32_t const dimension)
{
    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        vertex_count_ * sizeof(terrain_vertex),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* const vertices{staging_map.as<terrain_vertex>()};
    for (uint32_t z{}; z != dimension; ++z)
    {
        for (uint32_t x{}; x != dimension; ++x)
        {
            vertices[z * dimension + x] = terrain_vertex{
                .position = {cppext::as_fp(x), cppext::as_fp(z)}};
        }
    }

    renderer->transfer_buffer(staging_buffer, vertex_buffer_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}
