#ifndef NIKU_CAMERA_INCLUDED
#define NIKU_CAMERA_INCLUDED

#include <vkrndr_camera.hpp>

namespace niku
{
    class [[nodiscard]] camera : public vkrndr::camera
    {
    public:
        camera();

        camera(glm::vec3 const& position, float aspect_ratio);

        camera(camera const&) = default;

        camera(camera&&) = default;

    public:
        ~camera() override = default;

    public:
        virtual void update() = 0;

    public: // vkrndr::camera overrides
        void set_aspect_ratio(float aspect_ratio) override;

        [[nodiscard]] float aspect_ratio() const override;

        void set_position(glm::vec3 const& position) override;

        [[nodiscard]] glm::vec3 const& position() const override;

    public:
        camera& operator=(camera const&) = default;

        camera& operator=(camera&&) noexcept = default;

    protected:
        glm::vec3 position_;
        float aspect_ratio_;
    };
} // namespace niku
#endif
