target("03_HelloTriangle")
    set_kind("binary")
    
    -- Depend on Common library
    add_deps("VibeVulkanCommon")

    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32")
    end
    
    -- Include source files
    add_files("src/*.cpp")
    
    -- Add rule to compile GLSL shaders to SPIR-V
    -- Note: Requires glslc (from Vulkan SDK) in PATH
    after_build(function (target)
        local srcdir = path.join(os.projectdir(), "VibeVulkanRenderer/Lessons/03_HelloTriangle/src")
        local bindir = target:targetdir()
        
        -- Compile vertex shader
        os.exec("glslc " .. path.join(srcdir, "shader.vert") .. " -o " .. path.join(bindir, "vert.spv"))
        
        -- Compile fragment shader
        os.exec("glslc " .. path.join(srcdir, "shader.frag") .. " -o " .. path.join(bindir, "frag.spv"))
    end)
