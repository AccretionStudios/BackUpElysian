project "Imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    location (VcxDir)
    targetdir (BinDir)
    objdir (IntDir)
    debugdir (BinDir)

    files {
        (ThirdPartyDir .. "imgui/**.h"),
        (ThirdPartyDir .. "imgui/**.cpp"),
        (ThirdPartyDir .. "ImGuizmo/**.cpp"),
        (ThirdPartyDir .. "ImGuizmo/**.h"),
    }

    includedirs {
        IncludePath["ThirdParty"],
        IncludePath["GLFW"],
        IncludePath["Vulkan"]
    }

    libdirs {
        LibPath["GLFW"],
        LibPath["Vulkan"]
    }

    links {
        "vulkan-1",
        "glfw3"
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

