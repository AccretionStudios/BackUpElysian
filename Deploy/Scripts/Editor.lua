project "Editor"
    kind "ConsoleApp"
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
        IncludePath["Editor"],
        
        IncludePath["Vulkan"],
        IncludePath["GLFW"],
        IncludePath["ThirdParty"],
    }

    links {
        "Runtime",
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