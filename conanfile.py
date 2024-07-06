from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

require_conan_version = ">=2.0"

class SoilConan(ConanFile):
    name = "soil"
    settings = "os", "compiler", "build_type", "arch"
    version = "0.1"

    exports_sources = "cmake", "src", "CMakeLists.txt", "LICENSE"

    def requirements(self):
        self.requires("boost/1.85.0")
        self.requires("entt/3.13.0")
        self.requires("fmt/11.0.0")
        self.requires("freetype/2.13.2")
        self.requires("glm/cci.20230113")
        self.requires("imgui/1.90.8-docking")
        self.requires("joltphysics/5.0.0")
        self.requires("sdl/2.30.4")
        self.requires("spdlog/1.14.1")
        self.requires("stb/cci.20240213")
        self.requires("tinygltf/2.9.0")
        self.requires("vulkan-headers/1.3.268.0")
        self.requires("vulkan-loader/1.3.268.0")
        self.requires("vulkan-memory-allocator/3.1.0")

    def build_requirements(self):
        self.tool_requires("cmake/[^3.27]")
        self.test_requires("catch2/[^3.6.0]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = "ConanPresets.json"
        tc.generate()

        cmake = CMakeDeps(self)
        cmake.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

