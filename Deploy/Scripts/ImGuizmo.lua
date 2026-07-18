project "ImGuizmo"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    location (VcxDir)
    targetdir (BinDir)
    objdir (IntDir)
    debugdir (BinDir)

    files {
        (ThirdPartyDir .. "ImGuizmo/**.cpp"),
        (ThirdPartyDir .. "ImGuizmo/**.h"),
    }

    includedirs {
        IncludePath["ThirdParty"],
    }

    links {
        "imgui"
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

