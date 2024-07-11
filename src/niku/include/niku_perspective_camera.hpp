#ifndef NIKU_PERSPECTIVE_CAMERA_INCLUDED
#define NIKU_PERSPECTIVE_CAMERA_INCLUDED

#include <vkrndr_camera.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace niku
{
    class [[nodiscard]] perspective_camera : public vkrndr::camera
    {
    public:
        perspective_camera();

        perspective_camera(glm::vec3 const& position,
            float fov,
            glm::vec3 const& world_up,
            glm::vec2 const& near_far_planes,
            glm::vec2 const& yaw_pitch,
            float aspect_ratio);

        perspective_camera(perspective_camera const&) = default;

        perspective_camera(perspective_camera&&) noexcept = default;

    public:
        ~perspective_camera() override = default;

    public:
        void update();

        void update(glm::vec3 const& position,
            float fov,
            glm::vec3 const& world_up,
            glm::vec2 const& near_far_planes,
            glm::vec2 const& yaw_pitch,
            float aspect_ratio);

    public: // vkrndr::camera overrides
        [[nodiscard]] float aspect_ratio() const override;

        [[nodiscard]] glm::vec3 const& position() const override;

        [[nodiscard]] glm::mat4 const& view_matrix() const override;

        [[nodiscard]] glm::mat4 const& projection_matrix() const override;

        [[nodiscard]] glm::mat4 const& view_projection_matrix() const override;

    public:
        perspective_camera& operator=(perspective_camera const&) = default;

        perspective_camera& operator=(perspective_camera&&) noexcept = default;

    private:
        void calculate_view_projection_matrices();

    protected:
        glm::vec3 position_;
        glm::vec3 world_up_;
        glm::vec2 near_far_planes_;
        glm::vec2 yaw_pitch_;
        float aspect_ratio_;
        float fov_;

        glm::vec3 up_direction_;
        glm::vec3 front_direction_;
        glm::vec3 right_direction_;

        glm::mat4 view_matrix_;
        glm::mat4 projection_matrix_;
        glm::mat4 view_projection_matrix_;
    };
} // namespace niku

#endif
