#include <VibeApp.h>
#include <iostream>
#include <vector>

using namespace Vibe;
using namespace Microsoft::WRL;

class InitDX12App : public VibeApp {
public:
    InitDX12App(HINSTANCE hInstance);
    ~InitDX12App();

    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;

private:
    // DX12 核心对象
    ComPtr<IDXGIFactory4> m_dxgiFactory;    // 图形基础设施工厂(用于创建SwapChain等)
    ComPtr<ID3D12Device>  m_d3dDevice;      // 逻辑设备(显卡接口)
    ComPtr<ID3D12Fence>   m_fence;          // 围栏(CPU/GPU同步)

    // 命令提交相关
    ComPtr<ID3D12CommandQueue>        m_commandQueue;       // 命令队列
    ComPtr<ID3D12CommandAllocator>    m_commandAllocator;   // 命令分配器
    ComPtr<ID3D12GraphicsCommandList> m_commandList;        // 命令列表

    // 交换链相关
    ComPtr<IDXGISwapChain> m_swapChain;
    static const int SwapChainBufferCount = 2; // 双重缓冲
    int m_currBackBuffer = 0;
    
    // 渲染目标资源 (Back Buffers)
    ComPtr<ID3D12Resource> m_swapChainBuffers[SwapChainBufferCount];
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap; // RTV 描述符堆 (存放渲染目标的指针数组)
    UINT m_rtvDescriptorSize = 0;           // RTV 描述符大小

    // 视口与裁剪矩形
    D3D12_VIEWPORT m_screenViewport; 
    D3D12_RECT m_scissorRect;

    // 其它
    UINT64 m_currentFence = 0;

private:
    void InitDX12();
    void InitCommandObjects();
    void InitSwapChain();
    void InitRtvAndDsvDescriptorHeaps(); // Lesson 02 暂不处理 DSV (深度缓冲)
};

InitDX12App::InitDX12App(HINSTANCE hInstance) : VibeApp(hInstance) {
    m_MainWndCaption = L"Lesson 02: Init DX12 & Clean Screen";
}

InitDX12App::~InitDX12App() {
    // 等待 GPU 处理完所有命令后再析构
    if (m_d3dDevice != nullptr) {
        // 简单暴力等待(实际项目中需要完善同步机制)
        // FlushCommandQueue(); 
    }
}

bool InitDX12App::Initialize() {
    // 1. 初始化窗口
    if (!VibeApp::Initialize())
        return false;

    // 2. 初始化 DX12
    InitDX12();

    // 3. 可以在这里做第一次resize
    
    return true;
}

void InitDX12App::InitDX12() {
    HRESULT hardwareResult;

#if defined(_DEBUG)
    // 开启 DX12 调试层
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
        }
    }
#endif

    // 1. 创建 Factory
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

    // 2. 创建 Device (尝试创建硬件设备)
    hardwareResult = D3D12CreateDevice(
        nullptr,             // 默认适配器
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_d3dDevice)
    );

    // 如果硬件设备创建失败，回退到 WARP (软件光栅化)
    if (FAILED(hardwareResult)) {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(
            pWarpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_d3dDevice)
        ));
    }

    // 3. 创建 Fence
    ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    // 4. 获取描述符大小 (不同 GPU 可能不同)
    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 5. 初始化其它组件
    InitCommandObjects();
    InitSwapChain();
    InitRtvAndDsvDescriptorHeaps();
}

void InitDX12App::InitCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    
    ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    
    ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator)));

    ThrowIfFailed(m_d3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(), // 关联分配器
        nullptr,                  // 初始 PSO
        IID_PPV_ARGS(&m_commandList)));

    // 命令列表创建后默认是“录制”状态，我们先关闭它，第一次使用时再 Reset
    m_commandList->Close();
}

void InitDX12App::InitSwapChain() {
    // 释放之前的 SwapChain (如果是重新创建)
    m_swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = m_ClientWidth;
    sd.BufferDesc.Height = m_ClientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = SwapChainBufferCount;
    sd.OutputWindow = m_hMainWnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // SwapChain 需要 CommandQueue 来执行 Present
    ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
        m_commandQueue.Get(),
        &sd,
        &m_swapChain
    ));
}

void InitDX12App::InitRtvAndDsvDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    // 将 SwapChain 中的 Buffer 绑定到 Heap
    // D3D12_CPU_DESCRIPTOR_HANDLE 是 CPU 端的指针
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        // 获取 Buffer
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i])));
        
        // 创建 Render Target View (RTV)
        m_d3dDevice->CreateRenderTargetView(m_swapChainBuffers[i].Get(), nullptr, rtvHeapHandle);

        // 指针偏移到下一个插槽
        rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void InitDX12App::OnUpdate() {
    // 逻辑更新
}

void InitDX12App::OnRender() {
    // 1. 重置命令分配器和命令列表
    // (在实际复杂项目中，需要确保 GPU 执行完此分配器中的命令后才能 Reset)
    // 这里为了简单，假设是同步的，或者我们在下一课加围栏同步
    
    // 借用简单的 Flush (强制同步) 来保证安全 - 下一课会优化
    // ...

    ThrowIfFailed(m_commandAllocator->Reset());
    
    // 复用 CommandList
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    // 2. 将资源状态从 Present 切换到 RenderTarget
    // 这需要 Resource Barrier
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        m_swapChainBuffers[m_currBackBuffer].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierToRT);

    // 3. 设置 Viewport 和 Scissor
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width = static_cast<float>(m_ClientWidth);
    m_screenViewport.Height = static_cast<float>(m_ClientHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;
    m_scissorRect = { 0, 0, m_ClientWidth, m_ClientHeight };

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // 4. 清除屏幕 (Record Clear Command)
    // 获取当前 BackBuffer 的 RTV 句柄
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currBackBuffer,
        m_rtvDescriptorSize);

    // 设置渲染目标
    m_commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

    // 执行清除
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 蓝色
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    
    // 5. 将资源状态切换回 Present
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        m_swapChainBuffers[m_currBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierToPresent);

    // 6. 结束录制
    ThrowIfFailed(m_commandList->Close());

    // 7. 提交命令列表
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 8. 交换缓冲区 (Present)
    ThrowIfFailed(m_swapChain->Present(1, 0)); // 1 = 垂直同步

    // 9. 更新 BackBuffer 索引
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    // !! 极其重要的同步：等待 GPU 完成 !!
    // 注意：这是一种效率极低的方法（Flush），仅用于 Lesson 02 演示
    // 为了防止 CommandAllocator 被过早 Reset。
    
    // 向队列插入一个 Fence 点
    m_currentFence++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

    // 如果 GPU 还没执行到这个点，CPU 等待
    if (m_fence->GetCompletedValue() < m_currentFence) {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        InitDX12App theApp(hInstance);
        if(!theApp.Initialize())
            return 0;
        
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
