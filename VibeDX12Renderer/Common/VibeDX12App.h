#pragma once
#include "VibeApp.h"

namespace Vibe {
    class VibeDX12App : public VibeApp {
    public:
        VibeDX12App(HINSTANCE hInstance);
        virtual ~VibeDX12App();

        virtual bool Initialize() override;

    protected:
        // 核心资源
        ComPtr<IDXGIFactory4> m_dxgiFactory;
        ComPtr<ID3D12Device>  m_d3dDevice;
        ComPtr<ID3D12Fence>   m_fence;
        
        ComPtr<ID3D12CommandQueue>        m_commandQueue;
        ComPtr<ID3D12CommandAllocator>    m_commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;

        ComPtr<IDXGISwapChain> m_swapChain;
        
        static const int SwapChainBufferCount = 2;
        int m_currBackBuffer = 0;
        ComPtr<ID3D12Resource> m_swapChainBuffers[SwapChainBufferCount];
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12DescriptorHeap> m_dsvHeap; // 深度缓冲区堆
        
        UINT m_rtvDescriptorSize = 0;
        UINT m_dsvDescriptorSize = 0;

        // 深度/模板缓冲资源
        ComPtr<ID3D12Resource> m_depthStencilBuffer;
        DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 同步相关
        UINT64 m_currentFence = 0;

        // 视口
        D3D12_VIEWPORT m_screenViewport; 
        D3D12_RECT m_scissorRect;

    protected:
        // 初始化流程拆解
        void InitDX12();
        void InitCommandObjects();
        void InitSwapChain();
        void InitRtvHeap();
        void InitDepthStencilBuffer();
        
        // 渲染辅助
        void FlushCommandQueue();
        D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
        D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
        ID3D12Resource* CurrentBackBuffer() const;
    };
}
