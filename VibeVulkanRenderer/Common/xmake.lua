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
    add_requires("vulkan-headers", "vulkan-loader")
    add_packages("vulkan-headers", "vulkan-loader")
    
    -- Try to find Vulkan SDK from environment
    if os.getenv("VULKAN_SDK") then
        add_includedirs("$(env VULKAN_SDK)/Include")
        add_linkdirs("$(env VULKAN_SDK)/Lib")
        add_links("vulkan-1")
    end
