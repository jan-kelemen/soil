#include <terrain_renderer.hpp>

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
    constexpr uint32_t max_chunks{100};

    struct [[nodiscard]] camera_uniform final
    {
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct [[nodiscard]] chunk_uniform final
    {
        glm::mat4 model;
    };

    struct [[nodiscard]] push_constants final
    {
        uint32_t lod;
        uint32_t chunk;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(soil::terrain_vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(soil::terrain_vertex, position)}};

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
        VkDescriptorSetLayoutBinding camera_uniform_binding{};
        camera_uniform_binding.binding = 0;
        camera_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_binding.descriptorCount = 1;
        camera_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding chunk_uniform_binding{};
        chunk_uniform_binding.binding = 1;
        chunk_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        chunk_uniform_binding.descriptorCount = 1;
        chunk_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding heightmap_storage_binding{};
        heightmap_storage_binding.binding = 2;
        heightmap_storage_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        heightmap_storage_binding.descriptorCount = 1;
        heightmap_storage_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array const bindings{camera_uniform_binding,
            chunk_uniform_binding,
            heightmap_storage_binding};

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
        VkDescriptorBufferInfo const camera_uniform_info,
        VkDescriptorBufferInfo const chunk_uniform_info,
        VkDescriptorBufferInfo const heightmap_storage_info)
    {
        VkWriteDescriptorSet camera_uniform_write{};
        camera_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        camera_uniform_write.dstSet = descriptor_set;
        camera_uniform_write.dstBinding = 0;
        camera_uniform_write.dstArrayElement = 0;
        camera_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_write.descriptorCount = 1;
        camera_uniform_write.pBufferInfo = &camera_uniform_info;

        VkWriteDescriptorSet chunk_uniform_write{};
        chunk_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        chunk_uniform_write.dstSet = descriptor_set;
        chunk_uniform_write.dstBinding = 1;
        chunk_uniform_write.dstArrayElement = 0;
        chunk_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        chunk_uniform_write.descriptorCount = 1;
        chunk_uniform_write.pBufferInfo = &chunk_uniform_info;

        VkWriteDescriptorSet heightmap_uniform_write{};
        heightmap_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        heightmap_uniform_write.dstSet = descriptor_set;
        heightmap_uniform_write.dstBinding = 2;
        heightmap_uniform_write.dstArrayElement = 0;
        heightmap_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        heightmap_uniform_write.descriptorCount = 1;
        heightmap_uniform_write.pBufferInfo = &heightmap_storage_info;

        std::array const descriptor_writes{camera_uniform_write,
            chunk_uniform_write,
            heightmap_uniform_write};

        vkUpdateDescriptorSets(device->logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

soil::terrain_renderer::terrain_renderer(vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer,
    vkrndr::vulkan_image* const color_image,
    vkrndr::vulkan_image* const depth_buffer,
    vkrndr::vulkan_buffer* const heightmap_buffer,
    uint32_t const chunk_dimension)
    : device_{device}
    , renderer_{renderer}
    , color_image_{color_image}
    , depth_buffer_{depth_buffer}
    , chunk_dimension_{chunk_dimension}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
{
    auto const max_lod{static_cast<uint32_t>(round(log2(chunk_dimension))) + 1};
    for (uint32_t i{}; i != max_lod; ++i)
    {
        fill_index_buffer(chunk_dimension, i);
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

    frame_data_ =
        cppext::cycled_buffer<frame_resources>{renderer->image_count(),
            renderer->image_count()};
    for (auto const& [index, data] :
        std::views::enumerate(frame_data_.as_span()))
    {
        auto const camera_uniform_buffer_size{sizeof(camera_uniform)};
        data.camera_uniform = create_buffer(device_,
            camera_uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.camera_uniform_map =
            vkrndr::map_memory(device, data.camera_uniform.allocation);

        auto const chunk_uniform_buffer_size{
            sizeof(chunk_uniform) * max_chunks};
        data.chunk_uniform = create_buffer(device_,
            chunk_uniform_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.chunk_uniform_map =
            vkrndr::map_memory(device, data.chunk_uniform.allocation);

        create_descriptor_sets(device_,
            descriptor_set_layout_,
            renderer->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        DISABLE_WARNING_PUSH
        DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

        bind_descriptor_set(device_,
            data.descriptor_set,
            VkDescriptorBufferInfo{.buffer = data.camera_uniform.buffer,
                .offset = 0,
                .range = camera_uniform_buffer_size},
            VkDescriptorBufferInfo{.buffer = data.chunk_uniform.buffer,
                .offset = 0,
                .range = chunk_uniform_buffer_size},
            VkDescriptorBufferInfo{.buffer = heightmap_buffer->buffer,
                .offset = 0,
                .range = heightmap_buffer->size});

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

        unmap_memory(device_, &data.chunk_uniform_map);
        destroy(device_, &data.chunk_uniform);

        unmap_memory(device_, &data.camera_uniform_map);
        destroy(device_, &data.camera_uniform);
    }

    destroy(device_, pipeline_.get());
    pipeline_ = nullptr;

    vkDestroyDescriptorSetLayout(device_->logical,
        descriptor_set_layout_,
        nullptr);

    for (auto& index_buffer : index_buffers_)
    {
        destroy(device_, &index_buffer.index_buffer);
    }
}

void soil::terrain_renderer::update(soil::perspective_camera const& camera)
{
    auto& cam_uniform{*frame_data_->camera_uniform_map.as<camera_uniform>()};
    cam_uniform.view = camera.view_matrix();
    cam_uniform.projection = camera.projection_matrix();
}

vkrndr::render_pass_guard soil::terrain_renderer::begin_render_pass(
    VkImageView target_image,
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

    auto guard{render_pass.begin(command_buffer, render_area)};

    vkrndr::bind_pipeline(command_buffer,
        *pipeline_,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        std::span<VkDescriptorSet const>{&frame_data_->descriptor_set, 1});

    return guard;
}

void soil::terrain_renderer::draw(VkCommandBuffer command_buffer,
    uint32_t const lod,
    vkrndr::vulkan_buffer* const vertex_buffer,
    glm::mat4 const& model)
{
    if (frame_data_->last_bound_vertex_buffer != vertex_buffer)
    {
        VkDeviceSize const zero_offset{0};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &vertex_buffer->buffer,
            &zero_offset);
        frame_data_->last_bound_vertex_buffer = vertex_buffer;
    }

    if (auto it{std::ranges::find(index_buffers_, lod, &lod_index_buffer::lod)};
        it != std::cend(index_buffers_))
    {
        push_constants const constants{.lod = lod,
            .chunk = frame_data_->current_chunk_};

        auto* const ch_uniform{
            frame_data_->chunk_uniform_map.as<chunk_uniform>()};
        ch_uniform[frame_data_->current_chunk_].model = model;

        vkCmdPushConstants(command_buffer,
            *pipeline_->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(push_constants),
            &constants);

        vkCmdBindIndexBuffer(command_buffer,
            it->index_buffer.buffer,
            0,
            VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, it->index_count, 1, 0, 0, 0);

        ++frame_data_->current_chunk_;
    }
}

void soil::terrain_renderer::end_render_pass()
{
    frame_data_.cycle(
        [](frame_resources& current, frame_resources const&)
        {
            current.last_bound_vertex_buffer = nullptr;
            current.current_chunk_ = 0;
        });
}

void soil::terrain_renderer::draw_imgui() { ImGui::ShowMetricsWindow(); }

void soil::terrain_renderer::fill_index_buffer(uint32_t const dimension,
    uint32_t const lod)
{
    auto const lod_step{cppext::narrow<uint32_t>(1 << lod)};

    uint32_t const whole_index_count{(dimension - 1) * (dimension - 1) * 6};
    uint32_t const lod_index_count{whole_index_count / (lod_step * lod_step)};

    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        lod_index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto* indices{staging_map.as<uint32_t>()};
    for (uint32_t z{}; z != dimension - 1; z += lod_step)
    {
        for (uint32_t x{}; x != dimension - 1; x += lod_step)
        {
            auto const base_vertex{cppext::narrow<uint32_t>(z * dimension + x)};

            indices[0] = base_vertex;
            indices[1] =
                cppext::narrow<uint32_t>(base_vertex + dimension * lod_step);
            indices[2] = base_vertex + lod_step;
            indices[3] = base_vertex + lod_step;
            indices[4] =
                cppext::narrow<uint32_t>(base_vertex + dimension * lod_step);
            indices[5] = cppext::narrow<uint32_t>(
                base_vertex + dimension * lod_step + lod_step);

            indices += 6;
        }
    }

    vkrndr::vulkan_buffer lod_buffer{create_buffer(device_,
        lod_index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

    renderer_->transfer_buffer(staging_buffer, lod_buffer);

    index_buffers_.emplace_back(lod, lod_index_count, lod_buffer);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}
