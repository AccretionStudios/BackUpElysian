ThirdPartyDir = "%{wks.location}/../Source/ThirdParty/"
SrcDir = "%{wks.location}/../Source/"

BinDir = "%{wks.location}/Binaries/%{cfg.buildcfg}"
IntDir = "%{wks.location}/Intermediates/%{prj.name}/%{cfg.buildcfg}"
VcxDir = "%{wks.location}/Intermediates/%{prj.name}"

IncludePath = {}
IncludePath["Runtime"] = SrcDir .. "Runtime/"
IncludePath["Editor"] = SrcDir .. "Editor/"
IncludePath["GLFW"] = ThirdPartyDir .. "GLFW/include/"
IncludePath["ThirdParty"] = ThirdPartyDir
IncludePath["Vulkan"] = "$(VULKAN_SDK)/Include"

SrcDirProject = {}
SrcDirProject["Runtime"] = SrcDir .. "Runtime/"

LibPath = {}
LibPath["GLFW"] = ThirdPartyDir .. "GLFW/lib/"
LibPath["Vulkan"] = "$(VULKAN_SDK)/Lib"