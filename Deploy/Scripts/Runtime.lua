project "Runtime"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    location (VcxDir)
    targetdir (BinDir)
    objdir (IntDir)
    debugdir (BinDir)

    files {
        (SrcDir .. "%{prj.name}/**.hpp"),
        (SrcDir .. "%{prj.name}/**.h"),
        (SrcDir .. "%{prj.name}/**.cpp"),
    }

    includedirs {
        IncludePath["Runtime"],

        IncludePath["ThirdParty"],
        IncludePath["GLFW"],
        IncludePath["Vulkan"]
    }

    libdirs {
        LibPath["GLFW"],
        LibPath["Vulkan"]
    }

    links {
        "Imgui",
        "vulkan-1",
        "glfw3",
        "ThirdParty",
        "ImGuizmo"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Dist"
        defines { "DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"