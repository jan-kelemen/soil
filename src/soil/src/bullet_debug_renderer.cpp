#include <bullet_debug_renderer.hpp>

#include <bullet_adapter.hpp> // IWYU pragma: keep

#include <cppext_cycled_buffer.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkrndr_camera.hpp>
#include <vkrndr_render_pass.hpp>
#include <vulkan_buffer.hpp>
#include <vulkan_descriptors.hpp>
#include <vulkan_device.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_pipeline.hpp>
#include <vulkan_renderer.hpp>
#include <vulkan_utility.hpp>

#include <LinearMath/btVector3.h>

#include <imgui.h>

#include <fmt/base.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <optional>
#include <ranges>

// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <span>

namespace
{
    constexpr uint32_t max_vertex_count{1000000};

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] vertex final
    {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 color;
    };

    DISABLE_WARNING_POP

    struct [[nodiscard]] camera_uniform final
    {
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

soil::bullet_debug_renderer::bullet_debug_renderer(
    vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer,
    vkrndr::vulkan_image* color_image,
    vkrndr::vulkan_image* depth_buffer)
    : device_{device}
    , renderer_{renderer}
    , color_image_{color_image}
    , depth_buffer_{depth_buffer}
    , descriptor_set_layout_{create_descriptor_set_layout(device_)}
{
    line_pipeline_ = std::make_unique<vkrndr::vulkan_pipeline>(
        vkrndr::vulkan_pipeline_builder{device_,
            vkrndr::vulkan_pipeline_layout_builder{device_}
                .add_descriptor_set_layout(descriptor_set_layout_)
                .build(),
            renderer->image_format()}
            .add_shader(VK_SHADER_STAGE_VERTEX_BIT,
                "bullet_debug_line.vert.spv",
                "main")
            .add_shader(VK_SHADER_STAGE_FRAGMENT_BIT,
                "bullet_debug_line.frag.spv",
                "main")
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .with_dynamic_state(VK_DYNAMIC_STATE_LINE_WIDTH)
            .with_rasterization_samples(device_->max_msaa_samples)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_depth_test(depth_buffer_->format, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build());

    frame_data_ =
        cppext::cycled_buffer<frame_resources>{renderer->image_count(),
            renderer->image_count()};
    for (auto const& [index, data] :
        std::views::enumerate(frame_data_.as_span()))
    {
        auto const vertex_buffer_size{max_vertex_count * sizeof(vertex)};
        data.vertex_buffer = vkrndr::create_buffer(device_,
            vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.vertex_map =
            vkrndr::map_memory(device, data.vertex_buffer.allocation);

        auto const uniform_buffer_size{sizeof(camera_uniform)};
        data.vertex_uniform = create_buffer(device_,
            uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.uniform_map =
            vkrndr::map_memory(device, data.vertex_uniform.allocation);

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

soil::bullet_debug_renderer::~bullet_debug_renderer()
{
    for (auto& data : frame_data_.as_span())
    {
        vkFreeDescriptorSets(device_->logical,
            renderer_->descriptor_pool(),
            1,
            &data.descriptor_set);

        unmap_memory(device_, &data.uniform_map);
        destroy(device_, &data.vertex_uniform);

        unmap_memory(device_, &data.vertex_map);
        destroy(device_, &data.vertex_buffer);
    }

    destroy(device_, line_pipeline_.get());
    line_pipeline_ = nullptr;

    vkDestroyDescriptorSetLayout(device_->logical,
        descriptor_set_layout_,
        nullptr);
}

void soil::bullet_debug_renderer::set_camera_position(glm::vec3 const& position)
{
    camera_position_ = to_bullet(position);
}

void soil::bullet_debug_renderer::drawLine(btVector3 const& from,
    btVector3 const& to,
    btVector3 const& color)
{
    if (cull_geometry_)
    {
        if (from.distance(camera_position_) > cull_distance_ &&
            to.distance(camera_position_) > cull_distance_)
        {
            return;
        }
    }

    if (frame_data_->vertex_count < max_vertex_count)
    {
        auto* const vertices{frame_data_->vertex_map.as<vertex>()};

        vertices[frame_data_->vertex_count] =
            vertex{.position = from_bullet(from), .color = from_bullet(color)};
        vertices[frame_data_->vertex_count + 1] =
            vertex{.position = from_bullet(to), .color = from_bullet(color)};

        frame_data_->vertex_count += 2;
    }
}

void soil::bullet_debug_renderer::drawContactPoint(btVector3 const& PointOnB,
    btVector3 const& normalOnB,
    btScalar const distance,
    [[maybe_unused]] int const lifeTime,
    btVector3 const& color)
{
    drawLine(PointOnB, PointOnB + normalOnB * distance, color);
    btVector3 const ncolor{0, 0, 0};
    drawLine(PointOnB, PointOnB + normalOnB * 0.01f, ncolor);
}

void soil::bullet_debug_renderer::reportErrorWarning(char const* warningString)
{
    spdlog::error("{}", warningString);
}

void soil::bullet_debug_renderer::draw3dText(
    [[maybe_unused]] btVector3 const& location,
    [[maybe_unused]] char const* textString)
{
    spdlog::info("Text '{}' on {}", textString, location);
}

void soil::bullet_debug_renderer::setDebugMode(int debugMode)
{
    debug_mode_ = debugMode;
}

int soil::bullet_debug_renderer::getDebugMode() const { return debug_mode_; }

void soil::bullet_debug_renderer::update(vkrndr::camera const& camera,
    [[maybe_unused]] float const delta_time)
{
    *frame_data_->uniform_map.as<camera_uniform>() = {
        .view = camera.view_matrix(),
        .projection = camera.projection_matrix()};
}

void soil::bullet_debug_renderer::draw(VkImageView target_image,
    VkCommandBuffer command_buffer,
    VkRect2D const render_area)
{
    if (frame_data_->vertex_count > 0)
    {
        vkrndr::render_pass render_pass;

        render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            target_image,
            std::nullopt,
            color_image_->view);
        render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer_->view);

        {
            // cppcheck-suppress unreadVariable
            auto const guard{render_pass.begin(command_buffer, render_area)};

            VkDeviceSize const zero_offset{0};
            vkCmdBindVertexBuffers(command_buffer,
                0,
                1,
                &frame_data_->vertex_buffer.buffer,
                &zero_offset);

            vkrndr::bind_pipeline(command_buffer,
                *line_pipeline_,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                0,
                std::span<VkDescriptorSet const>{&frame_data_->descriptor_set,
                    1});

            vkCmdSetLineWidth(command_buffer, 4.0f);

            vkCmdDraw(command_buffer, frame_data_->vertex_count, 1, 0, 0);
        }
    }

    frame_data_.cycle([](auto const&, auto& next) { next.vertex_count = 0; });
}

void soil::bullet_debug_renderer::draw_imgui()
{
    ImGui::Begin("Bullet Physics");
    ImGui::Checkbox("Cull geometry", &cull_geometry_);
    if (cull_geometry_)
    {
        ImGui::SliderFloat("Cull distance", &cull_distance_, 0.0f, 50.0f);
    }
    ImGui::End();
}
