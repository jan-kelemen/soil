#include <terrain_renderer.hpp>

#include <heightmap.hpp>
#include <perspective_camera.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkrndr_render_pass.hpp>
#include <vulkan_buffer.hpp>
#include <vulkan_descriptors.hpp>
#include <vulkan_device.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_pipeline.hpp>
#include <vulkan_renderer.hpp>
#include <vulkan_utility.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>

// IWYU pragma: no_include <filesystem>

namespace
{
    struct [[nodiscard]] vertex final
    {
        glm::vec2 position;
    };

    struct [[nodiscard]] transform final
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct [[nodiscard]] push_constants final
    {
        uint32_t lod;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, position)}};

        return descriptions;
    }

    [[nodiscard]] VkSampler create_texture_sampler(
        vkrndr::vulkan_device const* const device,
        uint32_t const mip_levels)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device->physical, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = cppext::as_fp(mip_levels);

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device->logical, &sampler_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::vulkan_device const* const device)
    {
        VkDescriptorSetLayoutBinding vertex_uniform_binding{};
        vertex_uniform_binding.binding = 0;
        vertex_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertex_uniform_binding.descriptorCount = 1;
        vertex_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding vertex_storage_binding{};
        vertex_storage_binding.binding = 1;
        vertex_storage_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vertex_storage_binding.descriptorCount = 1;
        vertex_storage_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array const bindings{vertex_uniform_binding,
            vertex_storage_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device->logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
    }

    void bind_descriptor_set(vkrndr::vulkan_device const* const device,
        VkDescriptorSet const& descriptor_set,
        VkDescriptorBufferInfo const vertex_uniform_info,
        VkDescriptorBufferInfo const vertex_storage_info)
    {
        VkWriteDescriptorSet vertex_uniform_write{};
        vertex_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertex_uniform_write.dstSet = descriptor_set;
        vertex_uniform_write.dstBinding = 0;
        vertex_uniform_write.dstArrayElement = 0;
        vertex_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertex_uniform_write.descriptorCount = 1;
        vertex_uniform_write.pBufferInfo = &vertex_uniform_info;

        VkWriteDescriptorSet vertex_storage_write{};
        vertex_storage_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertex_storage_write.dstSet = descriptor_set;
        vertex_storage_write.dstBinding = 1;
        vertex_storage_write.dstArrayElement = 0;
        vertex_storage_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vertex_storage_write.descriptorCount = 1;
        vertex_storage_write.pBufferInfo = &vertex_storage_info;

        std::array const descriptor_writes{vertex_uniform_write,
            vertex_storage_write};

        vkUpdateDescriptorSets(device->logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

soil::terrain_renderer::terrain_renderer(vkrndr::vulkan_device* device,
    vkrndr::vulkan_renderer* renderer,
    vkrndr::vulkan_image* color_image,
    vkrndr::vulkan_image* depth_buffer,
    heightmap const& heightmap)
    : device_{device}
    , renderer_{renderer}
    , color_image_{color_image}
    , depth_buffer_{depth_buffer}
    , vertex_count_{cppext::narrow<uint32_t>(
          heightmap.dimension() * heightmap.dimension())}
    , vertex_buffer_{create_buffer(device,
          vertex_count_ * sizeof(vertex),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , heightmap_{create_buffer(device,
          vertex_count_ * sizeof(float),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
{
    fill_heightmap(heightmap);
    fill_vertex_buffer(heightmap.dimension(), heightmap.dimension());

    auto const max_lod{
        static_cast<uint32_t>(round(log2(heightmap.dimension() - 1)))};
    for (uint32_t i{}; i != max_lod; ++i)
    {
        fill_index_buffer(heightmap.dimension(), heightmap.dimension(), i);
    }

    pipeline_ = std::make_unique<vkrndr::vulkan_pipeline>(
        vkrndr::vulkan_pipeline_builder{device_,
            vkrndr::vulkan_pipeline_layout_builder{device_}
                .add_descriptor_set_layout(descriptor_set_layout_)
                .add_push_constants({.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = sizeof(push_constants)})
                .build(),
            renderer->image_format()}
            .add_shader(VK_SHADER_STAGE_VERTEX_BIT, "terrain.vert.spv", "main")
            .add_shader(VK_SHADER_STAGE_FRAGMENT_BIT,
                "terrain.frag.spv",
                "main")
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(device_->max_msaa_samples)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_depth_test(depth_buffer_->format)
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build());

    float const center_offset{cppext::as_fp(heightmap.dimension() - 1) / 2.0f};

    // Heightmap values range from [0, 1], bullet recenters it to [-0.5, 0.5]
    glm::vec3 offset{center_offset, 0.5f, center_offset};

    // Adjust offsets for scaling of heightmap
    offset *= -heightmap.scaling();

    glm::mat4 model_matrix{1.0f};
    model_matrix = glm::translate(model_matrix, offset);
    model_matrix = glm::scale(model_matrix, heightmap.scaling());

    glm::mat4 const normal_matrix{glm::transpose(glm::inverse(model_matrix))};

    frame_data_ =
        cppext::cycled_buffer<frame_resources>{renderer->image_count(),
            renderer->image_count()};
    for (auto const& [index, data] :
        std::views::enumerate(frame_data_.as_span()))
    {
        auto const uniform_buffer_size{sizeof(transform)};
        data.vertex_uniform = create_buffer(device_,
            uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.vertex_uniform_map =
            vkrndr::map_memory(device, data.vertex_uniform.allocation);
        data.vertex_uniform_map.as<transform>()->model = model_matrix;

        create_descriptor_sets(device_,
            descriptor_set_layout_,
            renderer->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        DISABLE_WARNING_PUSH
        DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

        bind_descriptor_set(device_,
            data.descriptor_set,
            VkDescriptorBufferInfo{.buffer = data.vertex_uniform.buffer,
                .offset = 0,
                .range = uniform_buffer_size},
            VkDescriptorBufferInfo{.buffer = heightmap_.buffer,
                .offset = 0,
                .range = vertex_count_ * sizeof(float)});

        DISABLE_WARNING_POP
    }
}

soil::terrain_renderer::~terrain_renderer()
{
    for (auto& data : frame_data_.as_span())
    {
        vkFreeDescriptorSets(device_->logical,
            renderer_->descriptor_pool(),
            1,
            &data.descriptor_set);

        unmap_memory(device_, &data.vertex_uniform_map);
        destroy(device_, &data.vertex_uniform);
    }

    destroy(device_, pipeline_.get());
    pipeline_ = nullptr;

    vkDestroyDescriptorSetLayout(device_->logical,
        descriptor_set_layout_,
        nullptr);

    destroy(device_, &heightmap_);

    for (auto& [lod, idx_buffer] : lod_index_buffers_)
    {
        destroy(device_, &idx_buffer.second);
    }

    destroy(device_, &vertex_buffer_);
}

void soil::terrain_renderer::update(soil::perspective_camera const& camera,
    [[maybe_unused]] float const delta_time)
{
    auto& uniform{*frame_data_->vertex_uniform_map.as<transform>()};
    uniform.view = camera.view_matrix();
    uniform.projection = camera.projection_matrix();
}

void soil::terrain_renderer::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkRect2D const render_area)
{
    vkrndr::render_pass render_pass;

    render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image,
        VkClearValue{{{0.0f, 0.0f, 0.0f, 1.f}}},
        color_image_->view);
    render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        depth_buffer_->view,
        VkClearValue{.depthStencil = {1.0f, 0}});

    {
        // cppcheck-suppress unreadVariable
        auto const guard{render_pass.begin(command_buffer, render_area)};

        vkrndr::bind_pipeline(command_buffer,
            *pipeline_,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            std::span<VkDescriptorSet const>{&frame_data_->descriptor_set, 1});

        VkDeviceSize const zero_offset{0};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &vertex_buffer_.buffer,
            &zero_offset);

        for (auto& [lod, count_buffer] : lod_index_buffers_)
        {
            push_constants const constants{.lod = lod};

            vkCmdPushConstants(command_buffer,
                *pipeline_->pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push_constants),
                &constants);

            vkCmdBindIndexBuffer(command_buffer,
                count_buffer.second.buffer,
                0,
                VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(command_buffer, count_buffer.first, 1, 0, 0, 0);
        }
    }

    frame_data_.cycle();
}

void soil::terrain_renderer::draw_imgui() { ImGui::ShowMetricsWindow(); }

void soil::terrain_renderer::fill_heightmap(heightmap const& heightmap)
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

    renderer_->transfer_buffer(staging_buffer, heightmap_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}

void soil::terrain_renderer::fill_vertex_buffer(size_t const width,
    size_t const height)
{
    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        vertex_count_ * sizeof(vertex),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* const vertices{staging_map.as<vertex>()};
    for (size_t z{}; z != height; ++z)
    {
        for (size_t x{}; x != width; ++x)
        {
            vertices[z * width + x] =
                vertex{.position = {cppext::as_fp(x), cppext::as_fp(z)}};
        }
    }

    renderer_->transfer_buffer(staging_buffer, vertex_buffer_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}

void soil::terrain_renderer::fill_index_buffer(size_t const width,
    size_t const height,
    uint32_t const lod)
{
    auto const lod_step{cppext::narrow<uint32_t>(1 << lod)};

    uint32_t const whole_index_count{
        cppext::narrow<uint32_t>((width - 1) * (height - 1) * 6)};
    uint32_t const lod_index_count{whole_index_count / (lod_step * lod_step)};

    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        lod_index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* indices{staging_map.as<uint32_t>()};
    for (size_t z{}; z != height - 1; z += lod_step)
    {
        for (size_t x{}; x != width - 1; x += lod_step)
        {
            auto const base_vertex{cppext::narrow<uint32_t>(z * width + x)};

            indices[0] = base_vertex;
            indices[1] =
                cppext::narrow<uint32_t>(base_vertex + width * lod_step);
            indices[2] = base_vertex + lod_step;
            indices[3] = base_vertex + lod_step;
            indices[4] =
                cppext::narrow<uint32_t>(base_vertex + width * lod_step);
            indices[5] = cppext::narrow<uint32_t>(
                base_vertex + width * lod_step + lod_step);

            indices += 6;
        }
    }

    vkrndr::vulkan_buffer lod_buffer{create_buffer(device_,
        lod_index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

    renderer_->transfer_buffer(staging_buffer, lod_buffer);

    lod_index_buffers_.emplace(lod,
        std::make_pair(lod_index_count, lod_buffer));

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}
