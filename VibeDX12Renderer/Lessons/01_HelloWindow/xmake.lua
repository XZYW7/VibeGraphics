target("01_HelloWindow")
    set_kind("binary")
    
    -- 依赖 Common 库
    add_deps("VibeCommon")

    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32")
    end
    
    -- 包含源码
    add_files("src/*.cpp")
