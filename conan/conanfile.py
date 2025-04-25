from conan import ConanFile
from conan.tools.build import can_run, check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, cmake_layout
from conan.tools.files import copy, update_conandata
from conan.tools.scm import Git

import os


class GraphicsConan(ConanFile):
    name = "realtime_rendering_prototypes"
    license = "MIT"
    author = "adnn"
    url = "https://github.com/Adnn/rtr_prototypes"
    description = "Prototypes from Real-Time Rendering 4th edition"
    topics = ("graphics", "3D")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    generators = "CMakeToolchain"
    revision_mode = "scm"

    def requirements(self):
        self.requires("graphics/a842a03bf7@adnn", transitive_headers=True)
        self.requires("handy/97edf2bb4f@adnn", transitive_headers=True)
        self.requires("math/ee8b6fb1ed@adnn", transitive_headers=True)

        self.requires("assimp/5.4.3", transitive_headers=False)
        self.requires("imgui/1.91.5-docking", transitive_headers=True)
        # Note: we do not want spdlog to be a public dependency
        self.requires("spdlog/1.15.1", transitive_headers=False)


    # There exist automatic alternatives.
    # see: https://docs.conan.io/2.0/reference/conanfile/methods/config_options.html?highlight=auto_shared_fpic
    def config_options(self):
        if self.settings.get_safe("os") == "Windows":
            self.options.rm_safe("fPIC")


    # see: https://github.com/conan-io/conan/issues/7530#issuecomment-1420634751
    def configure(self):
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")


    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, "20")


    # Handled at the profile level for the moment
    #def tool_requires(self):
    #    self.tool_requires("cmake/[>=3.31]")


    def layout(self):
        # The root of the project is one level above
        self.folders.root = ".."
        cmake_layout(self)


    def export(self):
        git = Git(self, self.recipe_folder)
        # Save the url and commit in conandata.yml
        # Unsafe atm since it is missing the repository argument,
        # so we save the coordinates manually
        #git.coordinates_to_conandata()
        url, commit = git.get_url_and_commit(repository=True)
        update_conandata(self, {"scm": {"url": url, "commit": commit}})


    def source(self):
        # we recover the saved url and commit from conandata.yml and use them to get sources
        git = Git(self)
        git.checkout_from_conandata_coordinates()
        git.run("submodule update --init")


    def generate(self):
        # the imgui package is designed this way: consumer has to import desired backends.
        # see: https://blog.conan.io/2019/06/26/An-introduction-to-the-Dear-ImGui-library.html
        # imports() has been removed from Conan 2 (and the blog post above is updated accordingly)
        # see: https://docs.conan.io/en/1.66/migrating_to_2.0/recipes.html#removed-imports-method
        imgui_package = os.path.join(self.dependencies["imgui"].package_folder, "res", "bindings")
        destination = os.path.join(self.build_folder, "conan_imports", "imgui_bindings")
        copy(self, "imgui_impl_glfw.h",           src=imgui_package, dst=destination)
        copy(self, "imgui_impl_glfw.cpp",         src=imgui_package, dst=destination)
        copy(self, "imgui_impl_opengl3.h",        src=imgui_package, dst=destination)
        copy(self, "imgui_impl_opengl3.cpp",      src=imgui_package, dst=destination)
        copy(self, "imgui_impl_opengl3_loader.h", src=imgui_package, dst=destination)

        deps = CMakeDeps(self)
        # Detect component name problem as soon as find_package()
        deps.check_components_exist = True
        deps.generate()


    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if can_run(self):
            cmake.test()


    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))


    def package_info(self):
        pass
