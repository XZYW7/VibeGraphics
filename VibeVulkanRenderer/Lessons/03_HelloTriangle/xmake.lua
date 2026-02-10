target("03_HelloTriangle")
    set_kind("binary")
    
    -- Depend on Common library
    add_deps("VibeVulkanCommon")

    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32")
    end
    
    -- Include source files
    add_files("src/*.cpp")
    
    -- Use the shared rule for shader compilation
    add_rules("utils.glsl2spv")
