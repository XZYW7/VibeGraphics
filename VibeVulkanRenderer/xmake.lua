set_project("VibeVulkanRenderer")
set_version("0.0.1")
set_xmakever("2.5.0")

-- Set common compile options
add_rules("mode.debug", "mode.release")
set_languages("cxx20")

-- Windows specific settings
if is_os("windows") then 
    add_defines("UNICODE", "_UNICODE") 
    add_cxflags("/utf-8") -- Force UTF-8 encoding to avoid C4819 warning
    add_defines("VK_USE_PLATFORM_WIN32_KHR")
end

-- Add shaderc globally so all lessons can use it for compilation
add_requires("shaderc")

-- Define a rule for compiling GLSL shaders to SPIR-V
rule("utils.glsl2spv")
    on_load(function (target)
        -- Ensure shaderc package is associated with the target so we can find it
        target:add("packages", "shaderc")
    end)
    after_build(function (target)
        import("core.project.task")
        
        -- Determine source directory (default to src/ inside the target's script dir)
        local srcdir = path.join(target:scriptdir(), "src")
        if not os.exists(srcdir) then
            srcdir = target:scriptdir()
        end
        local bindir = target:targetdir()

        -- Find glslc executable
        local glslc = "glslc"
        local shaderc = target:pkg("shaderc")
        if shaderc then
            local bin = path.join(shaderc:installdir(), "bin")
            if is_host("windows") then 
                glslc = path.join(bin, "glslc.exe")
            else
                glslc = path.join(bin, "glslc")
            end
        elseif os.getenv("VULKAN_SDK") then
             local sdk = os.getenv("VULKAN_SDK")
             if is_host("windows") then
                 glslc = path.join(sdk, "Bin", "glslc.exe")
             else
                 glslc = path.join(sdk, "bin", "glslc")
             end
        end

        print("Compiling shaders for " .. target:name() .. " with: " .. glslc)

        -- Helper to compile a single file
        local function compile_shader(src_file, out_file)
            if os.exists(src_file) then
                print("  -> Compiling " .. path.filename(src_file))
                -- Use os.runv for safe execution with arguments
                try
                {
                    function ()
                        os.runv(glslc, {src_file, "-o", out_file})
                    end,
                    catch
                    {
                        function (e)
                            print("Error compiling shader: " .. e)
                            raise(e)
                        end
                    }
                }
            end
        end

        -- Scan for shaders
        local shader_files = os.files(path.join(srcdir, "*.vert"))
        for _, file in ipairs(os.files(path.join(srcdir, "*.frag"))) do
            table.insert(shader_files, file)
        end
        for _, file in ipairs(os.files(path.join(srcdir, "*.comp"))) do
            table.insert(shader_files, file)
        end
        for _, file in ipairs(os.files(path.join(srcdir, "*.geom"))) do
            table.insert(shader_files, file)
        end
        for _, file in ipairs(os.files(path.join(srcdir, "*.tesc"))) do
            table.insert(shader_files, file)
        end
        for _, file in ipairs(os.files(path.join(srcdir, "*.tese"))) do
            table.insert(shader_files, file)
        end

        for _, file in ipairs(shader_files) do
            local filename = path.filename(file)
            local outfile = ""
            
            -- Special naming convention for tutorial files
            if filename == "shader.vert" then
                outfile = path.join(bindir, "vert.spv")
            elseif filename == "shader.frag" then
                outfile = path.join(bindir, "frag.spv")
            else
                -- Default: filename.spv
                outfile = path.join(bindir, filename .. ".spv")
            end
            
            compile_shader(file, outfile)
        end
    end)

-- Include sub-projects
includes("Common")
includes("Lessons/*")
