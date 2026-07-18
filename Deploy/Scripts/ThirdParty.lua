project "ThirdParty"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    location (VcxDir)
    targetdir (BinDir)
    objdir (IntDir)
    debugdir (BinDir)

    files {
        (ThirdPartyDir .. "ufbx/**.h"),
        (ThirdPartyDir .. "ufbx/**.c"),

        (ThirdPartyDir .. "entt/**.hpp"),
        (ThirdPartyDir .. "glm/**.hpp"),
        (ThirdPartyDir .. "json/**.hpp"),
        (ThirdPartyDir .. "tinyobjloader/**.h"),
        (ThirdPartyDir .. "VMA/**.h"),
        (ThirdPartyDir .. "GLFW/**.h"),
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

