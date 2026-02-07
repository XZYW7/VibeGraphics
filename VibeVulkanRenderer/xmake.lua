set_project("VibeVulkanRenderer")
set_version("0.0.1")
set_xmakever("2.5.0")

-- Set common compile options
add_rules("mode.debug", "mode.release")
set_languages("cxx20")

-- Windows specific settings
if is_os("windows") then 
    add_defines("UNICODE", "_UNICODE") 
    add_cxflags("/utf-8") -- Force UTF-8 encoding to avoid C4819 warning
    add_defines("VK_USE_PLATFORM_WIN32_KHR")
end

-- Include sub-projects
includes("Common")
includes("Lessons/*")
