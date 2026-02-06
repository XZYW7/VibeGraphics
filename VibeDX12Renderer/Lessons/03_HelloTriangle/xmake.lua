target("03_HelloTriangle")
    set_kind("binary")
    
    add_deps("VibeCommon")
    
    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32", "d3d12", "dxgi", "d3dcompiler")
    end
    
    add_files("src/*.cpp")
    
    -- 将 shader 文件拷贝到输出目录
    after_build(function (target)
        -- $(scriptdir) 指向当前 xmake.lua 所在目录
        os.cp("$(scriptdir)/src/shaders.hlsl", target:targetdir())
    end)
