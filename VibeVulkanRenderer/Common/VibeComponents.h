#pragma once

// Windows Headers
#include <windows.h>
#include <windowsx.h>

// Vulkan Headers
#include <vulkan/vulkan.h>

// DirectXMath (for math operations, still useful for Vulkan)
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

// STL
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>
#include <optional>

// Helper Macros
#define VK_CHECK(x) \
    do { \
        VkResult err = x; \
        if (err != VK_SUCCESS) { \
            std::stringstream ss; \
            ss << "Vulkan error: " << err << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

namespace Vibe {
    // Utility function to throw on error
    inline void ThrowIfFailed(VkResult result) {
        if (result != VK_SUCCESS) {
            std::stringstream ss;
            ss << "Vulkan operation failed with error code: " << result;
            throw std::runtime_error(ss.str());
        }
    }
}
