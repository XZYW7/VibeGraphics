target("02_InitDX12")
    set_kind("binary")
    
    add_deps("VibeCommon")
    
    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32", "d3d12", "dxgi", "d3dcompiler")
    end
    
    add_files("src/*.cpp")
