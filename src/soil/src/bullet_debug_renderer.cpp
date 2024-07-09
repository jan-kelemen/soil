#include <bullet_debug_renderer.hpp>

#include <bullet_adapter.hpp> // IWYU pragma: keep

#include <cppext_cyclic_stack.hpp>
#include <cppext_numeric.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_depth_buffer.hpp>
#include <vulkan_device.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_pipeline.hpp>
#include <vulkan_renderer.hpp>

#include <LinearMath/btVector3.h>

#include <fmt/base.h>

#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <ranges>

// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <span>

namespace
{
    constexpr uint32_t max_vertex_count{1000};

    struct [[nodiscard]] vertex final
    {
        glm::vec3 position;
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
        };

        return descriptions;
    }
} // namespace

soil::bullet_debug_renderer::bullet_debug_renderer(
    vkrndr::vulkan_device* const device,
    vkrndr::vulkan_renderer* const renderer)
    : device_{device}
    , renderer_{renderer}
    , depth_buffer_{
          vkrndr::create_depth_buffer(device, renderer->extent(), false)}
{
    line_pipeline_ = std::make_unique<vkrndr::vulkan_pipeline>(
        vkrndr::vulkan_pipeline_builder{device_,
            vkrndr::vulkan_pipeline_layout_builder{device_}.build(),
            renderer->image_format()}
            .add_shader(VK_SHADER_STAGE_VERTEX_BIT,
                "bullet_debug_line.vert.spv",
                "main")
            .add_shader(VK_SHADER_STAGE_FRAGMENT_BIT,
                "bullet_debug_line.frag.spv",
                "main")
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .with_rasterization_samples(device_->max_msaa_samples)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_depth_test(depth_buffer_.format)
            .build());

    frame_data_ = cppext::cyclic_stack<frame_resources>{renderer->image_count(),
        renderer->image_count()};
    for (auto const& [index, data] :
        std::views::enumerate(frame_data_.as_span()))
    {
        data.vertex_buffer = vkrndr::create_buffer(device_,
            max_vertex_count * sizeof(vertex) * renderer->image_count(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.vertex_map =
            vkrndr::map_memory(device, data.vertex_buffer.allocation);
    }
}

soil::bullet_debug_renderer::~bullet_debug_renderer()
{
    for (auto& data : frame_data_.as_span())
    {
        unmap_memory(device_, &data.vertex_map);
        destroy(device_, &data.vertex_buffer);
    }

    destroy(device_, line_pipeline_.get());
    line_pipeline_ = nullptr;

    destroy(device_, &depth_buffer_);
}

void soil::bullet_debug_renderer::drawLine(btVector3 const& from,
    btVector3 const& to,
    btVector3 const& color)
{
    spdlog::info("Line from {} to {} with color {}", from, to, color);

    auto& frame_data{frame_data_.top()};
    if (frame_data.vertex_count < max_vertex_count)
    {
        frame_data.vertex_map.as<vertex>()[frame_data.vertex_count].position =
            from_bullet(from);
        frame_data.vertex_map.as<vertex>()[frame_data.vertex_count + 1]
            .position = from_bullet(to);

        frame_data.vertex_count += 2;
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

VkClearValue soil::bullet_debug_renderer::clear_color()
{
    return {{{0.0f, 0.0f, 0.0f, 1.f}}};
}

VkClearValue soil::bullet_debug_renderer::clear_depth()
{
    return {.depthStencil = {1.0f, 0}};
}

vkrndr::vulkan_image* soil::bullet_debug_renderer::depth_image()
{
    return &depth_buffer_;
}

void soil::bullet_debug_renderer::resize(VkExtent2D const extent)
{
    destroy(device_, &depth_buffer_);
    depth_buffer_ = vkrndr::create_depth_buffer(device_, extent, false);
}

void soil::bullet_debug_renderer::draw(VkCommandBuffer command_buffer,
    VkExtent2D const extent)
{
    auto& frame_data{frame_data_.top()};

    VkDeviceSize const zero_offset{0};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &frame_data.vertex_buffer.buffer,
        &zero_offset);

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
        *line_pipeline_,
        VK_PIPELINE_BIND_POINT_GRAPHICS);

    vkCmdDraw(command_buffer, frame_data.vertex_count, 1, 0, 0);

    frame_data_.cycle();
    frame_data_.top().vertex_count = 0;
}

void soil::bullet_debug_renderer::draw_imgui() { }
