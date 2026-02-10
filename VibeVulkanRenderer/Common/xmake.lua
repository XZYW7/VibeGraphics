add_requires("vulkan-headers", "vulkan-loader")

target("VibeVulkanCommon")
    set_kind("static")
    
    -- Include directories
    add_includedirs(".", {public = true}) 
    
    -- Add source files
    add_files("**.cpp")
    add_headerfiles("**.h")

    -- Windows specific settings
    if is_os("windows") then
        add_syslinks("user32", "shell32")
    end
    
    -- Vulkan SDK
    
    add_packages("vulkan-headers", "vulkan-loader", {public = true})
    
