set_project("VibeDX12Renderer")
set_version("0.0.1")
set_xmakever("2.5.0")

-- 设置通用编译选项
add_rules("mode.debug", "mode.release")
set_languages("cxx20")

-- DX12 仅限 Windows
if is_os("windows") then 
    add_defines("UNICODE", "_UNICODE") 
    add_cxflags("/utf-8") -- 解决 C4819 警告，强制使用 UTF-8 编码
end

-- 包含子项目
includes("Common")
includes("Lessons/*")
