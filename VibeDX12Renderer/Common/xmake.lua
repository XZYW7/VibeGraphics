target("VibeCommon")
    set_kind("static")
    
    -- 包含目录
    add_includedirs(".", {public = true}) 
    
    -- 添加源码
    add_files("**.cpp")
    add_headerfiles("**.h")

    -- Windows 下链接 DX12 核心库
    if is_os("windows") then
        add_syslinks("d3d12", "dxgi", "d3dcompiler", "user32", "shell32")
    end
