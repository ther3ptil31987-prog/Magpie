from conan import ConanFile


class MagpieDependencies(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "MSBuildDeps"
    default_options = {
        "spdlog/*:header_only": True,
        "spdlog/*:wchar_filenames": True,
        "spdlog/*:no_exceptions": True,
        "minhook/*:shared": False,
    }

    def requirements(self):
        for dependency in (
            "fmt/12.1.0", "spdlog/1.17.0", "parallel-hashmap/2.0.0",
            "rapidjson/cci.20230929", "kuba-zip/0.3.2", "muparser/2.3.5",
            "yas/7.1.0", "imgui/1.91.8", "rapidhash/1.0",
        ):
            self.requires(dependency)
        # The existing ARM64 build uses the normal Win32 foreground path.
        if str(self.settings.arch) in ("x86", "x86_64"):
            self.requires("minhook/1.3.4")
