from conan import ConanFile
from conan.tools.build import can_run, check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, cmake_layout
from conan.tools.files import copy, update_conandata
from conan.tools.scm import Git

import os


class GraphicsConan(ConanFile):
    name = "rtr_prototypes"
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
        self.requires("graphics/0.1.0@adnn/develop", transitive_headers=True)


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
