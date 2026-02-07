target("12-1_Tessellation_TexturedTerrain")
    set_kind("binary")
    add_deps("VibeVulkanCommon")
    if is_os("windows") then
        add_syslinks("user32", "shell32", "gdi32")
    end
    add_files("src/*.cpp")
