# VibeGraphics

A comprehensive graphics programming learning framework featuring step-by-step tutorials for DirectX 12 and Vulkan APIs.

## Projects

### VibeDX12Renderer
Progressive DirectX 12 lessons covering fundamental to advanced rendering concepts.

- ✅ Complete implementation of 13 lessons
- Topics: Window creation, DX12 initialization, rendering, lighting, shaders, and more
- Platform: Windows

### VibeVulkanRenderer
Progressive Vulkan lessons following a similar structure to DX12 lessons.

- ✅ Lesson 00-03 fully implemented
- 🚧 Lessons 04-12 have stub implementations (to be completed)
- Topics: Window creation, Vulkan initialization, rendering, lighting, shaders, and more
- Platform: Windows (with potential for cross-platform expansion)

## Getting Started

### Prerequisites
- Windows 10 or later
- Visual Studio 2019+ or compatible C++ compiler with C++20 support
- xmake build system
- Windows SDK
- DirectX 12 SDK (for VibeDX12Renderer)
- Vulkan SDK (for VibeVulkanRenderer)

### Building

```bash
# Build DirectX 12 lessons
cd VibeDX12Renderer
xmake

# Build Vulkan lessons
cd VibeVulkanRenderer
xmake
```

## Learning Path

Both renderers follow a similar progression:

1. **Hello Window** - Window creation and message loop
2. **API Initialization** - Setting up DX12/Vulkan
3. **Hello Triangle** - First rendered geometry
4. **Depth Buffering** - 3D rendering fundamentals
5. **Textures** - Texture mapping
6. **Camera Control** - Interactive camera
7. **Lighting** - Illumination models
8. **Advanced Topics** - Blending, stenciling, geometry shaders, compute shaders, tessellation

## Contributing

Contributions are welcome! Feel free to submit pull requests or open issues.

## License

See LICENSE file for details.