include "Deploy/Scripts/Path.lua"

workspace "Elysian-Engine"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }
    startproject "Editor"
    location "Build"

    filter "system:windows"
        buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus", "/utf-8" }

    group "Engine"
        include "Deploy/Scripts/Editor.lua"
        include "Deploy/Scripts/Runtime.lua"

    group "ThirdParty"
        include "Deploy/Scripts/Imgui.lua"
        include "Deploy/Scripts/ImGuizmo.lua"
        include "Deploy/Scripts/ThirdParty.lua"