#include <terrain_renderer.hpp>

#include <heightmap.hpp>

#include <cppext_pragma_warning.hpp>

#include <vulkan_depth_buffer.hpp>
#include <vulkan_descriptors.hpp>
#include <vulkan_device.hpp>
#include <vulkan_pipeline.hpp>
#include <vulkan_renderer.hpp>
#include <vulkan_utility.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <ranges>

namespace
{
    constexpr uint32_t max_vertex_count{100000};

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] vertex final
    {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 color;
    };

    DISABLE_WARNING_POP

    struct [[nodiscard]] transform final
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
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
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, color)},
        };

        return descriptions;
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

        std::array const bindings{vertex_uniform_binding};

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
        VkDescriptorBufferInfo const vertex_uniform_info)
    {
        VkWriteDescriptorSet vertex_uniform_write{};
        vertex_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vertex_uniform_write.dstSet = descriptor_set;
        vertex_uniform_write.dstBinding = 0;
        vertex_uniform_write.dstArrayElement = 0;
        vertex_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertex_uniform_write.descriptorCount = 1;
        vertex_uniform_write.pBufferInfo = &vertex_uniform_info;

        std::array const descriptor_writes{vertex_uniform_write};

        vkUpdateDescriptorSets(device->logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

soil::terrain_renderer::terrain_renderer(vkrndr::vulkan_device* device,
    vkrndr::vulkan_renderer* renderer,
    heightmap const& heightmap)
    : device_{device}
    , renderer_{renderer}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
    , depth_buffer_{vkrndr::create_depth_buffer(device,
          renderer->extent(),
          false)}
    , vertex_count_{cppext::narrow<uint32_t>(
          heightmap.dimension() * heightmap.dimension())}
    , index_offset_{vertex_count_ * sizeof(vertex)}
    , index_count_{cppext::narrow<uint32_t>(
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
            .with_depth_test(depth_buffer_.format)
            .build());

    auto const vertex_buffer_size{
        vertex_count_ * sizeof(vertex) + index_count_ * sizeof(uint16_t)};
    vertex_index_buffer_ = vkrndr::create_buffer(device_,
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

        auto* const vertices{staging_map.as<vertex>()};
        auto const heightmap_data{heightmap.data()};
        for (size_t y{}; y != heightmap.dimension(); ++y)
        {
            for (size_t x{}; x != heightmap.dimension(); ++x)
            {
                vertices[y * heightmap.dimension() + x] = {
                    .position = {cppext::as_fp(x),
                        heightmap.value(x, y),
                        cppext::as_fp(y)},
                    .color = {1.0f, 1.0f, 1.0f}};
            }
        }

        auto* indices{staging_map.as<uint16_t>(index_offset_)};
        for (size_t z{}; z != heightmap.dimension() - 1; ++z)
        {
            for (size_t x{}; x != heightmap.dimension() - 1; ++x)
            {
                indices[0] = z * heightmap.dimension() + x;
                indices[1] = z * heightmap.dimension() + x + 1;
                indices[2] = (z + 1) * heightmap.dimension() + x;
                indices[3] = z * heightmap.dimension() + x + 1;
                indices[4] = (z + 1) * heightmap.dimension() + x;
                indices[5] = (z + 1) * heightmap.dimension() + x + 1;

                indices += 6;
            }
        }

        vkrndr::unmap_memory(device_, &staging_map);

        renderer_->transfer_buffer(staging_buffer, vertex_index_buffer_);

        destroy(device_, &staging_buffer);
    }

    float const center_offset{(heightmap.dimension() - 1) / 2.0f};

    // Heightmap values range from [0, 1], bullet recenters it to [-0.5, 0.5]
    glm::vec3 offset{center_offset, 0.5f, center_offset};

    // Adjust offsets for scaling of heightmap
    offset *= -heightmap.scaling();

    glm::mat4 model_matrix{1.0f};
    model_matrix = glm::translate(model_matrix, offset);
    model_matrix = glm::scale(model_matrix, heightmap.scaling());

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
        data.uniform_map =
            vkrndr::map_memory(device, data.vertex_uniform.allocation);
        data.uniform_map.as<transform>()->model = model_matrix;

        create_descriptor_sets(device_,
            descriptor_set_layout_,
            renderer->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        bind_descriptor_set(device_,
            data.descriptor_set,
            VkDescriptorBufferInfo{.buffer = data.vertex_uniform.buffer,
                .offset = 0,
                .range = uniform_buffer_size});
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

        unmap_memory(device_, &data.uniform_map);
        destroy(device_, &data.vertex_uniform);
    }

    destroy(device_, &vertex_index_buffer_);

    destroy(device_, pipeline_.get());
    pipeline_ = nullptr;

    vkDestroyDescriptorSetLayout(device_->logical,
        descriptor_set_layout_,
        nullptr);

    destroy(device_, &depth_buffer_);
}

VkClearValue soil::terrain_renderer::clear_color()
{
    return {{{0.0f, 0.0f, 0.0f, 1.f}}};
}

VkClearValue soil::terrain_renderer::clear_depth()
{
    return {.depthStencil = {1.0f, 0}};
}

vkrndr::vulkan_image* soil::terrain_renderer::depth_image()
{
    return &depth_buffer_;
}

void soil::terrain_renderer::resize(VkExtent2D extent)
{
    destroy(device_, &depth_buffer_);
    depth_buffer_ = vkrndr::create_depth_buffer(device_, extent, false);
}

void soil::terrain_renderer::update(vkrndr::camera const& camera,
    [[maybe_unused]] float delta_time)
{
    auto& uniform{*frame_data_->uniform_map.as<transform>()};
    uniform.view = camera.view_matrix();
    uniform.projection = camera.projection_matrix();
}

void soil::terrain_renderer::draw(VkCommandBuffer command_buffer,
    VkExtent2D extent)
{
    VkDeviceSize const zero_offset{0};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &vertex_index_buffer_.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        vertex_index_buffer_.buffer,
        index_offset_,
        VK_INDEX_TYPE_UINT16);

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(extent.width),
        .height = cppext::as_fp(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkrndr::bind_pipeline(command_buffer,
        *pipeline_,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        std::span<VkDescriptorSet const>{&frame_data_->descriptor_set, 1});

    vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 1);

    frame_data_.cycle();
}

void soil::terrain_renderer::draw_imgui() { }
