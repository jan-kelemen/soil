#include <terrain_renderer.hpp>

#include <heightmap.hpp>
#include <noise.hpp>
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
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>

// IWYU pragma: no_include <filesystem>

namespace
{
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
        uint32_t chunk_dimension;
        uint32_t terrain_dimension;
        uint32_t chunks_per_dimension;
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
                .format = VK_FORMAT_R32G32_UINT,
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

        VkDescriptorSetLayoutBinding normals_storage_binding{};
        normals_storage_binding.binding = 3;
        normals_storage_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        normals_storage_binding.descriptorCount = 1;
        normals_storage_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding textures_binding{};
        textures_binding.binding = 4;
        textures_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        textures_binding.descriptorCount = 1;
        textures_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding texture_sampler_binding{};
        texture_sampler_binding.binding = 5;
        texture_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        texture_sampler_binding.descriptorCount = 1;
        texture_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{camera_uniform_binding,
            chunk_uniform_binding,
            heightmap_storage_binding,
            normals_storage_binding,
            textures_binding,
            texture_sampler_binding};

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
        VkDescriptorBufferInfo const heightmap_storage_info,
        VkDescriptorBufferInfo const normals_storage_info,
        std::span<VkDescriptorImageInfo const> const textures_info,
        VkDescriptorImageInfo const texture_sampler_info)
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

        VkWriteDescriptorSet normals_uniform_write{};
        normals_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        normals_uniform_write.dstSet = descriptor_set;
        normals_uniform_write.dstBinding = 3;
        normals_uniform_write.dstArrayElement = 0;
        normals_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        normals_uniform_write.descriptorCount = 1;
        normals_uniform_write.pBufferInfo = &normals_storage_info;

        VkWriteDescriptorSet textures_write{};
        textures_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        textures_write.dstSet = descriptor_set;
        textures_write.dstBinding = 4;
        textures_write.dstArrayElement = 0;
        textures_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        textures_write.descriptorCount =
            vkrndr::count_cast(textures_info.size());
        textures_write.pImageInfo = textures_info.data();

        VkWriteDescriptorSet texture_sampler_write{};
        texture_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texture_sampler_write.dstSet = descriptor_set;
        texture_sampler_write.dstBinding = 5;
        texture_sampler_write.dstArrayElement = 0;
        texture_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        texture_sampler_write.descriptorCount = 1;
        texture_sampler_write.pImageInfo = &texture_sampler_info;

        std::array const descriptor_writes{camera_uniform_write,
            chunk_uniform_write,
            heightmap_uniform_write,
            normals_uniform_write,
            textures_write,
            texture_sampler_write};

        vkUpdateDescriptorSets(device->logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

soil::terrain_renderer::terrain_renderer(heightmap const& heightmap,
    vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer,
    vkrndr::vulkan_image* const color_image,
    vkrndr::vulkan_image* const depth_buffer,
    uint32_t const terrain_dimension,
    uint32_t const chunk_dimension)
    : device_{device}
    , renderer_{renderer}
    , color_image_{color_image}
    , depth_buffer_{depth_buffer}
    , terrain_dimension_{terrain_dimension}
    , chunk_dimension_{chunk_dimension}
    , chunks_per_dimension_{(terrain_dimension_ - 1) / (chunk_dimension_ - 1) +
          1}
    , heightmap_buffer_{create_buffer(device,
          heightmap.dimension() * heightmap.dimension() * sizeof(float),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , normal_buffer_{create_buffer(device,
          heightmap.dimension() * heightmap.dimension() * sizeof(glm::vec4),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , texture_mix_image_{create_texture_mix_image()}
    , texture_sampler_{create_texture_sampler(device_,
          texture_mix_image_.mip_levels)}
    , vertex_count_{chunk_dimension_ * chunk_dimension_}
    , vertex_buffer_{create_buffer(device,
          vertex_count_ * sizeof(terrain_vertex),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
{
    fill_heightmap(heightmap);
    fill_normals(heightmap);

    fill_vertex_buffer();

    auto const max_lod{static_cast<uint32_t>(round(log2(chunk_dimension))) + 1};
    for (uint32_t i{}; i != max_lod; ++i)
    {
        fill_index_buffer(chunk_dimension, i);
    }

    pipeline_ = std::make_unique<vkrndr::vulkan_pipeline>(
        vkrndr::vulkan_pipeline_builder{device_,
            vkrndr::vulkan_pipeline_layout_builder{device_}
                .add_descriptor_set_layout(descriptor_set_layout_)
                .add_push_constants({.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
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

        auto const chunk_uniform_buffer_size{sizeof(chunk_uniform) *
            chunks_per_dimension_ * chunks_per_dimension_};
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
        std::array const texture_info{
            VkDescriptorImageInfo{.imageView = texture_mix_image_.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

        VkDescriptorImageInfo const sampler_info{.sampler = texture_sampler_};
        DISABLE_WARNING_POP

        bind_descriptor_set(device_,
            data.descriptor_set,
            VkDescriptorBufferInfo{.buffer = data.camera_uniform.buffer,
                .offset = 0,
                .range = camera_uniform_buffer_size},
            VkDescriptorBufferInfo{.buffer = data.chunk_uniform.buffer,
                .offset = 0,
                .range = chunk_uniform_buffer_size},
            VkDescriptorBufferInfo{.buffer = heightmap_buffer_.buffer,
                .offset = 0,
                .range = heightmap_buffer_.size},
            VkDescriptorBufferInfo{.buffer = normal_buffer_.buffer,
                .offset = 0,
                .range = normal_buffer_.size},
            texture_info,
            sampler_info);
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

    destroy(device_, &vertex_buffer_);

    vkDestroySampler(device_->logical, texture_sampler_, nullptr);

    destroy(device_, &texture_mix_image_);

    destroy(device_, &normal_buffer_);
    destroy(device_, &heightmap_buffer_);
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

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &vertex_buffer_.buffer,
        &zero_offset);

    return guard;
}

void soil::terrain_renderer::draw(VkCommandBuffer command_buffer,
    uint32_t const lod,
    uint32_t const chunk_index,
    glm::mat4 const& model)
{
    if (auto it{std::ranges::find(index_buffers_, lod, &lod_index_buffer::lod)};
        it != std::cend(index_buffers_))
    {
        push_constants const constants{.lod = lod,
            .chunk = chunk_index,
            .chunk_dimension = chunk_dimension_,
            .terrain_dimension = terrain_dimension_,
            .chunks_per_dimension = chunks_per_dimension_};

        auto* const ch_uniform{
            frame_data_->chunk_uniform_map.as<chunk_uniform>()};
        ch_uniform[chunk_index].model = model;

        vkCmdPushConstants(command_buffer,
            *pipeline_->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(push_constants),
            &constants);

        vkCmdBindIndexBuffer(command_buffer,
            it->index_buffer.buffer,
            0,
            VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(command_buffer, it->index_count, 1, 0, 0, 0);
    }
}

void soil::terrain_renderer::end_render_pass() { frame_data_.cycle(); }

void soil::terrain_renderer::draw_imgui() { }

void soil::terrain_renderer::fill_heightmap(heightmap const& heightmap)
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

    renderer_->transfer_buffer(staging_buffer, heightmap_buffer_);

    destroy(device_, &staging_buffer);
}

void soil::terrain_renderer::fill_normals(heightmap const& heightmap)
{
    vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
        normal_buffer_.size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    vkrndr::mapped_memory staging_map{
        vkrndr::map_memory(device_, staging_buffer.allocation)};

    auto const face_normal = [](glm::vec3 const& point1,
                                 glm::vec3 const& point2,
                                 glm::vec3 const& point3)
    {
        glm::vec3 const edge1{point2 - point1};
        glm::vec3 const edge2{point3 - point1};

        // https://stackoverflow.com/a/57812028
        return glm::normalize(glm::cross(edge1, edge2));
    };

    memset(staging_map.as<void>(), 0, normal_buffer_.size);

    auto* const normals{staging_map.as<glm::vec4>()};
    for (uint32_t z{}; z != terrain_dimension_ - 1; ++z)
    {
        for (uint32_t x{}; x != terrain_dimension_ - 1; ++x)
        {
            auto const base_vertex{
                cppext::narrow<uint32_t>(z * terrain_dimension_ + x)};

            auto const fn1 = face_normal({x, heightmap.value(x, z), z},
                {x, heightmap.value(x, z + 1), z + 1},
                {x + 1, heightmap.value(x + 1, z), z});
            normals[base_vertex] += glm::vec4{fn1, 0.0f};
            normals[cppext::narrow<uint32_t>(
                base_vertex + terrain_dimension_)] += glm::vec4{fn1, 0.0f};
            normals[base_vertex + 1] += glm::vec4{fn1, 0.0f};

            auto const fn2 = face_normal({x + 1, heightmap.value(x + 1, z), z},
                {x, heightmap.value(x, z + 1), z + 1},
                {x + 1, heightmap.value(x + 1, z + 1), z + 1});

            normals[base_vertex + 1] += glm::vec4{fn2, 0.0f};
            normals[cppext::narrow<uint32_t>(
                base_vertex + terrain_dimension_)] += glm::vec4{fn2, 0.0f};
            normals[cppext::narrow<uint32_t>(
                base_vertex + terrain_dimension_ + 1)] += glm::vec4{fn2, 0.0f};
        }
    }

    for (size_t i{}; i != normal_buffer_.size / sizeof(glm::vec4); ++i)
    {
        normals[i] = glm::normalize(normals[i]);
    }

    unmap_memory(device_, &staging_map);

    renderer_->transfer_buffer(staging_buffer, normal_buffer_);

    destroy(device_, &staging_buffer);
}

vkrndr::vulkan_image soil::terrain_renderer::create_texture_mix_image()
{
    auto staging_buffer{vkrndr::create_buffer(device_,
        VkDeviceSize{terrain_dimension_} * terrain_dimension_,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    auto staging_map{map_memory(device_, staging_buffer.allocation)};
    generate_2d_noise(
        std::span{staging_map.as<std::byte>(), staging_buffer.size},
        terrain_dimension_);
    unmap_memory(device_, &staging_map);

    auto rv{renderer_->transfer_buffer_to_image(staging_buffer,
        VkExtent2D{terrain_dimension_, terrain_dimension_},
        VK_FORMAT_R8_UNORM,
        vkrndr::max_mip_levels(terrain_dimension_, terrain_dimension_))};
    destroy(device_, &staging_buffer);

    return rv;
}

void soil::terrain_renderer::fill_vertex_buffer()
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

    renderer_->transfer_buffer(staging_buffer, vertex_buffer_);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}

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

    vkrndr::vulkan_buffer const lod_buffer{create_buffer(device_,
        lod_index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

    renderer_->transfer_buffer(staging_buffer, lod_buffer);

    index_buffers_.emplace_back(lod, lod_index_count, lod_buffer);

    unmap_memory(device_, &staging_map);
    destroy(device_, &staging_buffer);
}
