# VibeVulkanRenderer

A step-by-step Vulkan learning framework, designed to parallel the DX12 lessons in VibeDX12Renderer.

## Overview

VibeVulkanRenderer provides a structured approach to learning Vulkan API through progressive lessons. Each lesson builds upon the previous one, introducing new concepts and techniques.

## Structure

```
VibeVulkanRenderer/
├── Common/                 # Shared code and utilities
│   ├── VibeApp.h/cpp      # Base application class
│   ├── VibeVulkanApp.h/cpp # Vulkan application base class
│   ├── VibeCamera.h/cpp   # Camera utilities
│   └── VibeComponents.h   # Common headers and utilities
├── Lessons/               # Progressive learning lessons
│   ├── 00_Cover/         # Introduction
│   ├── 01_HelloWindow/   # Window creation
│   ├── 02_InitVulkan/    # Vulkan initialization
│   ├── 03_HelloTriangle/ # First triangle
│   ├── 04_DepthBuffering/
│   ├── 05_Textures/
│   ├── 06_CameraControl/
│   ├── 07_Lighting/
│   ├── 08_Blending/
│   ├── 09_Stenciling/
│   ├── 10_GeometryShader/
│   ├── 11_ComputeShader/
│   └── 12_Tessellation/
└── xmake.lua             # Build configuration
```

## Lessons

### Completed Lessons

- **Lesson 00: Cover** - Overview of the learning path
- **Lesson 01: Hello Window** - Create a window using Win32 API
- **Lesson 02: Init Vulkan** - Initialize Vulkan instance, device, and swapchain
- **Lesson 03: Hello Triangle** - Render your first colored triangle

### Lessons To Be Implemented

- **Lesson 04: Depth Buffering** - Implement depth testing
- **Lesson 05: Textures** - Load and apply textures
- **Lesson 06: Camera Control** - Implement camera movement
- **Lesson 07: Lighting** - Basic lighting models
- **Lesson 08: Blending** - Transparency and blending
- **Lesson 09: Stenciling** - Stencil buffer effects
- **Lesson 10: Geometry Shader** - Advanced geometry processing
- **Lesson 11: Compute Shader** - GPU compute operations
- **Lesson 12: Tessellation** - Advanced surface subdivision

## Building

### Prerequisites

- **Vulkan SDK** - Download from [LunarG](https://vulkan.lunarg.com/)
- **xmake** - Build system
- **Windows SDK** - For Windows platform development
- **C++20 compatible compiler**

### Environment Setup

1. Install Vulkan SDK and ensure `VULKAN_SDK` environment variable is set
2. Ensure `glslc` (shader compiler) is in your PATH
3. Install xmake build system

### Build Instructions

```bash
# Build all lessons
xmake

# Build specific lesson
xmake build 03_HelloTriangle

# Run a lesson
xmake run 03_HelloTriangle
```

## Key Concepts

### Vulkan Initialization Flow

1. Create Instance
2. Create Surface (for window)
3. Pick Physical Device (GPU)
4. Create Logical Device
5. Create Swapchain
6. Create Image Views
7. Create Render Pass
8. Create Graphics Pipeline
9. Create Framebuffers
10. Create Command Buffers
11. Create Synchronization Objects

### Vulkan Rendering Loop

1. Wait for previous frame
2. Acquire next swapchain image
3. Record command buffer
4. Submit command buffer
5. Present image

## Differences from DX12

- **Explicit Synchronization**: Vulkan requires explicit management of synchronization
- **Verbose Setup**: More boilerplate code for initialization
- **SPIR-V Shaders**: Uses SPIR-V bytecode instead of HLSL
- **Render Passes**: Explicit render pass objects for efficient rendering
- **Cross-Platform**: Works on Windows, Linux, Android, etc.

## Resources

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Vulkan Specification](https://www.khronos.org/vulkan/)
- [Vulkan SDK Documentation](https://vulkan.lunarg.com/doc/sdk)

## Notes

- Lessons 04-12 have stub implementations and are marked for future completion
- Each lesson is self-contained and can be built independently
- Shader files (`.vert`, `.frag`) are compiled to SPIR-V (`.spv`) during build

## License

See LICENSE file in the root directory.
