target("00_Cover")
    set_kind("binary")
    
    -- Include source files
    add_files("src/*.cpp")
    
    -- Windows subsystem
    if is_os("windows") then
        add_ldflags("/SUBSYSTEM:CONSOLE")
    end
