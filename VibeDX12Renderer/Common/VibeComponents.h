#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <wrl.h> -- ComPtr
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace Vibe {
    // 简单的错误检查宏
    inline void ThrowIfFailed(HRESULT hr) {
        if (FAILED(hr)) {
            throw std::runtime_error("HRESULT Failed");
        }
    }
}
