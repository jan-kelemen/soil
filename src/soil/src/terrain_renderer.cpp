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
    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] vertex final
    {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 normal;
        alignas(16) glm::vec2 texture_coordinate;
        alignas(16) glm::vec3 tangent;
    };

    DISABLE_WARNING_POP

    struct [[nodiscard]] transform final
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 normal;
    };

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] view final
    {
        alignas(16) glm::vec3 camera_position;
        alignas(16) glm::vec3 light_position;
    };

    DISABLE_WARNING_POP

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
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, normal)},
            VkVertexInputAttributeDescription{.location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, texture_coordinate)},
            VkVertexInputAttributeDescription{.location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, tangent)},
        };

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

        VkDescriptorSetLayoutBinding view_uniform_binding{};
        view_uniform_binding.binding = 1;
        view_uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        view_uniform_binding.descriptorCount = 1;
        view_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding texture_image_binding{};
        texture_image_binding.binding = 2;
        texture_image_binding.descriptorCount = 1;
        texture_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_image_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        texture_image_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding normal_image_binding{};
        normal_image_binding.binding = 3;
        normal_image_binding.descriptorCount = 1;
        normal_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        normal_image_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding texture_sampler_binding{};
        texture_sampler_binding.binding = 4;
        texture_sampler_binding.descriptorCount = 1;
        texture_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        texture_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{vertex_uniform_binding,
            view_uniform_binding,
            texture_image_binding,
            normal_image_binding,
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
        VkDescriptorBufferInfo const vertex_uniform_info,
        VkDescriptorBufferInfo const view_uniform_info,
        VkDescriptorImageInfo const texture_image_info,
        VkDescriptorImageInfo const normal_image_info,
        VkDescriptorImageInfo const sampler_image_info)
    {
        VkWriteDescriptorSet vertex_uniform_write{};
        vertex_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertex_uniform_write.dstSet = descriptor_set;
        vertex_uniform_write.dstBinding = 0;
        vertex_uniform_write.dstArrayElement = 0;
        vertex_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertex_uniform_write.descriptorCount = 1;
        vertex_uniform_write.pBufferInfo = &vertex_uniform_info;

        VkWriteDescriptorSet view_uniform_write{};
        view_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        view_uniform_write.dstSet = descriptor_set;
        view_uniform_write.dstBinding = 1;
        view_uniform_write.dstArrayElement = 0;
        view_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        view_uniform_write.descriptorCount = 1;
        view_uniform_write.pBufferInfo = &view_uniform_info;

        VkWriteDescriptorSet texture_descriptor_write{};
        texture_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texture_descriptor_write.dstSet = descriptor_set;
        texture_descriptor_write.dstBinding = 2;
        texture_descriptor_write.dstArrayElement = 0;
        texture_descriptor_write.descriptorType =
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_descriptor_write.descriptorCount = 1;
        texture_descriptor_write.pImageInfo = &texture_image_info;

        VkWriteDescriptorSet normal_descriptor_write{};
        normal_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        normal_descriptor_write.dstSet = descriptor_set;
        normal_descriptor_write.dstBinding = 3;
        normal_descriptor_write.dstArrayElement = 0;
        normal_descriptor_write.descriptorType =
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        normal_descriptor_write.descriptorCount = 1;
        normal_descriptor_write.pImageInfo = &normal_image_info;

        VkWriteDescriptorSet sampler_descriptor_write{};
        sampler_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_descriptor_write.dstSet = descriptor_set;
        sampler_descriptor_write.dstBinding = 4;
        sampler_descriptor_write.dstArrayElement = 0;
        sampler_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_descriptor_write.descriptorCount = 1;
        sampler_descriptor_write.pImageInfo = &sampler_image_info;

        std::array const descriptor_writes{vertex_uniform_write,
            view_uniform_write,
            texture_descriptor_write,
            normal_descriptor_write,
            sampler_descriptor_write};

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
    , texture_{renderer->load_texture("coast_sand_rocks_02_diff_1k.jpg",
          VK_FORMAT_R8G8B8A8_SRGB)}
    , texture_normal_{renderer->load_texture(
          "coast_sand_rocks_02_nor_gl_1k.jpg",
          VK_FORMAT_R8G8B8A8_UNORM)}
    , texture_sampler_{create_texture_sampler(device,
          std::min(texture_.mip_levels, texture_normal_.mip_levels))}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
    , vertex_count_{cppext::narrow<uint32_t>(
          (heightmap.dimension() - 1) * (heightmap.dimension() - 1) * 6)}
{
    pipeline_ = std::make_unique<vkrndr::vulkan_pipeline>(
        vkrndr::vulkan_pipeline_builder{device_,
            vkrndr::vulkan_pipeline_layout_builder{device_}
                .add_descriptor_set_layout(descriptor_set_layout_)
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

    auto const vertex_buffer_size{vertex_count_ * sizeof(vertex)};
    vertex_buffer_ = vkrndr::create_buffer(device_,
        vertex_buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    {
        vkrndr::vulkan_buffer staging_buffer{vkrndr::create_buffer(device_,
            vertex_buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        vkrndr::mapped_memory staging_map{
            vkrndr::map_memory(device_, staging_buffer.allocation)};

        auto* vertices{staging_map.as<vertex>()};
        auto const create_vertex = [&heightmap](size_t const x, size_t const z)
        {
            DISABLE_WARNING_PUSH
            DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
            return vertex{.position = {cppext::as_fp(x),
                              heightmap.value(x, z),
                              cppext::as_fp(z)},
                .texture_coordinate = {cppext::as_fp(x % 2) * 5,
                    cppext::as_fp(z % 2) * 5}};
            DISABLE_WARNING_POP
        };
        for (size_t z{}; z != heightmap.dimension() - 1; ++z)
        {
            for (size_t x{}; x != heightmap.dimension() - 1; ++x)
            {
                vertices[0] = create_vertex(x, z);
                vertices[1] = create_vertex(x, z + 1);
                vertices[2] = create_vertex(x + 1, z);
                vertices[3] = create_vertex(x + 1, z);
                vertices[4] = create_vertex(x, z + 1);
                vertices[5] = create_vertex(x + 1, z + 1);

                vertices += 6;
            }
        }
        vertices = staging_map.as<vertex>();

        for (uint32_t i{}; i != vertex_count_; i += 3)
        {
            vertex const& point1{vertices[i]};
            vertex const& point2{vertices[i + 1]};
            vertex const& point3{vertices[i + 2]};

            glm::vec3 const edge1{point2.position - point1.position};
            glm::vec3 const edge2{point3.position - point1.position};

            // https://stackoverflow.com/a/57812028
            glm::vec3 const face_normal{
                glm::normalize(glm::cross(edge1, edge2))};

            glm::vec2 const delta_texture1{
                point2.texture_coordinate - point1.texture_coordinate};
            glm::vec2 const delta_texture2{
                point3.texture_coordinate - point1.texture_coordinate};

            float const f{1.0f /
                (delta_texture1.x * delta_texture2.y -
                    delta_texture2.x * delta_texture1.y)};

            glm::vec3 const tan{
                (edge1 * delta_texture2.y - edge2 * delta_texture1.y) * f};

            vertices[i].tangent = tan;
            vertices[i].normal = face_normal;
            vertices[i + 1].tangent = tan;
            vertices[i + 1].normal = face_normal;
            vertices[i + 2].tangent = tan;
            vertices[i + 2].normal = face_normal;
        }

        vkrndr::unmap_memory(device_, &staging_map);

        renderer_->transfer_buffer(staging_buffer, vertex_buffer_);

        destroy(device_, &staging_buffer);
    }

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
        data.vertex_uniform_map.as<transform>()->normal = normal_matrix;

        auto const view_buffer_size{sizeof(view)};
        data.view_uniform = create_buffer(device_,
            view_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.view_uniform_map =
            vkrndr::map_memory(device, data.view_uniform.allocation);

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
            VkDescriptorBufferInfo{.buffer = data.view_uniform.buffer,
                .offset = 0,
                .range = view_buffer_size},
            VkDescriptorImageInfo{.imageView = texture_.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.imageView = texture_normal_.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = texture_sampler_});

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

        unmap_memory(device_, &data.view_uniform_map);
        destroy(device_, &data.view_uniform);

        unmap_memory(device_, &data.vertex_uniform_map);
        destroy(device_, &data.vertex_uniform);
    }

    destroy(device_, &vertex_buffer_);

    destroy(device_, pipeline_.get());
    pipeline_ = nullptr;

    vkDestroyDescriptorSetLayout(device_->logical,
        descriptor_set_layout_,
        nullptr);

    destroy(device_, &texture_);
    destroy(device_, &texture_normal_);

    vkDestroySampler(device_->logical, texture_sampler_, nullptr);
}

void soil::terrain_renderer::update(soil::perspective_camera const& camera,
    [[maybe_unused]] float const delta_time)
{
    auto& uniform{*frame_data_->vertex_uniform_map.as<transform>()};
    uniform.view = camera.view_matrix();
    uniform.projection = camera.projection_matrix();

    auto& v{*frame_data_->view_uniform_map.as<view>()};
    v.camera_position = camera.position();
    v.light_position = camera.position() + camera.up_direction();
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

        VkDeviceSize const zero_offset{0};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &vertex_buffer_.buffer,
            &zero_offset);

        vkrndr::bind_pipeline(command_buffer,
            *pipeline_,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            std::span<VkDescriptorSet const>{&frame_data_->descriptor_set, 1});

        vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
    }

    frame_data_.cycle();
}

void soil::terrain_renderer::draw_imgui() { ImGui::ShowMetricsWindow(); }
