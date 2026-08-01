import os
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps
from conan.tools.files import copy

class RoR(ConanFile):
    name = "Rigs of Rods"
    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "ogre3d*:resourcemanager_strict": "off",
        "ogre3d*:profiling": "True"
    }

    def layout(self):
        self.folders.generators = os.path.join(self.folders.build, "generators")

    def requirements(self):
        self.requires("angelscript/2.35.1")
        self.requires("discord-rpc/3.4.0@anotherfoxguy/stable")
        self.requires("libcurl/8.2.1")
        self.requires("fmt/12.1.0")
        # Phase 2.0 - OGRE 1.11.6.1 -> 14.5.2, desktop first (ROADMAP 2.0).
        # Doing the engine jump here, against a known-good reference build, keeps
        # OGRE breakage and Android breakage from arriving at the same time and
        # becoming mutually unattributable.
        #
        # 14.5.2 is the same version Spike A proved on Android, and the whole
        # coupled set is already published on the RoR remote (the 2025.10 Caelum
        # and PagedGeometry builds are the OGRE-14-compatible rebuilds).
        # MyGUI stays at 3.4.0 for now, NOT because it is preferred but because
        # it is the only version with a prebuilt binary matching this toolchain.
        # 3.4.1 and 3.4.3 both fail when built from source - their recipe's
        # build() references CMake/Utils/PrecompiledHeader.cmake, which the
        # source tarball does not contain (FileNotFoundError, reproduced on
        # both). Pinning compiler.version to reach their older binaries was
        # tried and is worse: Conan then requests a vcvars toolset that is not
        # installed. See cmake/conan-profile-windows.txt.
        #
        # OPEN QUESTION for the OGRE 14 upgrade: this binary was built against
        # OGRE 1.11 headers, so it may be ABI-incompatible with 14.5.2. If the
        # link fails, the options are (a) fix the recipe locally and build from
        # source, or (b) bring the MyGUI removal forward from Phase 5 - it is
        # replacement-bound anyway because it is not touch-capable.
        self.requires("mygui/3.4.0@anotherfoxguy/stable")
        self.requires("ogre3d-caelum/2025.10@anotherfoxguy/stable")
        self.requires("ogre3d-pagedgeometry/2025.10@anotherfoxguy/stable")
        self.requires("ogre3d/14.5.2@anotherfoxguy/stable", force=True)
        self.requires("ois/1.4.1@rigsofrods/custom")
        self.requires("openal-soft/1.24.3")
        self.requires("openssl/3.6.3", force=True)
        self.requires("rapidjson/cci.20211112", force=True)
        self.requires("socketw/3.11.0@anotherfoxguy/stable")

        self.requires("jasper/4.2.4", override=True)
        self.requires("libpng/1.6.58", override=True)
        self.requires("libwebp/1.6.0", override=True)
        self.requires("zlib/1.3.2", override=True)
        self.requires("zziplib/0.13.78@anotherfoxguy/stable", override=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
        if self.settings.os == "Windows" and self.settings.build_type == "Release":
            deps.configuration = "RelWithDebInfo"
            deps.generate()

        for dep in self.dependencies.values():
            for f in dep.cpp_info.bindirs:
                self.cp_data(f)
            for f in dep.cpp_info.libdirs:
                self.cp_data(f)

    def cp_data(self, src):
        bindir = os.path.join(self.build_folder, "bin")
        copy(self, "*.dll", src, bindir, False)
        copy(self, "*.so*", src, bindir, False)
